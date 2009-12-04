/*
 * Copyright (c) 2005 Nickolay Kondrashov
 *
 * Based on examples from "USB HID for Linux USB"
 * by Brad Hards, Sigma Pty Ltd.
 *
 * This file is part of wizardpen-calibrate.
 *
 * Wizardpen-calibrate is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wizardpen-calibrate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wizardpen-calibrate; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <fcntl.h>
#include <linux/input.h>

/* max number of input events to read in one read call */
#define MAX_EVENTS 64

#define NUM_REGISTER_KEYS	3
__u16 register_keys[NUM_REGISTER_KEYS] = { BTN_LEFT, BTN_SIDE, BTN_TOUCH };

struct INPUT {
	int			fd;
	struct input_event	buffer[MAX_EVENTS];
	size_t			read_bytes;
	struct input_event	*event;
};

struct INPUT *open_input( char *filename )
{
	struct INPUT* input;
	input = (struct INPUT*)malloc( sizeof( struct INPUT ) );

	if( ( input->fd = open( filename, O_RDONLY ) ) < 0 ) {
		perror( "event device open" );
		exit( 1 );
	}

	input->read_bytes = 0;
	input->event = input->buffer;

	return input;
}

void close_input( struct INPUT* input )
{
	close( input->fd );
	free( input );
}

struct input_event* read_input( struct INPUT* input )
{
	input->event++;

	if(
		( (char*)input->event ) - ( (char*)input->buffer ) >=
		input->read_bytes
	) {

		input->read_bytes = read(
			input->fd,
			input->buffer,
			MAX_EVENTS * sizeof( struct input_event )
		);

		if( input->read_bytes < sizeof( struct input_event ) ) {
			perror( "short read" );
			exit( 1 );
		}

		input->event = input->buffer;
	}

	return input->event;
}

__u16 capture_key( struct INPUT* input, int *x, int *y, int num_key, __u16 *keys )
{
	struct input_event *event;

	int got_x = 0;
	int got_y = 0;
	int got_key = 0;
	__u16 key;
	int key_index;

	do {
		event = read_input( input );

		// DEBUG printf(
		// DEBUG 	"Event: time %ld.%06ld, type %d, code %d, value %d\n",
		// DEBUG 	event->time.tv_sec,
		// DEBUG 	event->time.tv_usec,
		// DEBUG 	event->type,
		// DEBUG 	event->code,
		// DEBUG 	event->value
		// DEBUG );

		if( event->type == EV_ABS ) {

			// remember position
			switch( event->code ) {

				case ABS_X:
				case ABS_Z:	/* Some tablets send ABS_Z instead of ABS_X */
					got_x = 1;
					*x = event->value;
				break;

				case ABS_Y:
				case ABS_RX:	/* Some tablets send ABS_RX instead of ABS_Y */
					got_y = 1;
					*y = event->value;
				break;
			}
		}

		if(
			event->type == EV_KEY && ( !event->value ) &&
			got_x && got_y
		) {	// some key released at some position

			key = event->code;

			for( key_index = 0; key_index < num_key; key_index++ ) {
				if( key == keys[key_index] ) {
					got_key = 1;
					break;
				}
			}
		}

	} while( !got_key );

	return key; /* key code */
}

int main( int argc, char **argv ) {

	char *filename;
	struct INPUT *input;
	int x1, y1;
	int x2, y2;
	int TopX, TopY;
	int BottomX, BottomY;

	if( argc != 2 ) {
		fprintf( stderr, "usage: %s event-device - probably /dev/input/event0\n", argv[0] );
		exit( 1 );
	}

	filename = argv[1];

	input = open_input( filename );

	fprintf(
		stderr,
		"\nPlease, press the stilus at ANY\n"
		"corner of your desired working area: "
	);
	capture_key( input, &x1, &y1, NUM_REGISTER_KEYS, register_keys );
	fprintf( stderr, "ok, got %d,%d\n", x1, y1 );

	fprintf(
		stderr,
		"\nPlease, press the stilus at OPPOSITE\n"
		"corner of your desired working area: "
	);
	capture_key( input, &x2, &y2, NUM_REGISTER_KEYS, register_keys );
	fprintf( stderr, "ok, got %d,%d\n", x2, y2 );

	if( x2 > x1 ) {
		TopX = x1;
		BottomX = x2;
	} else {
		TopX = x2;
		BottomX = x1;
	}

	if( y2 > y1 ) {
		TopY = y1;
		BottomY = y2;
	} else {
		TopY = y2;
		BottomY = y1;
	}

	fprintf(
		stderr,
		"\nAccording to your input you may put following\n"
		"lines into your XF86Config file:\n"
		"\n"
	);
	printf(
		"\tDriver\t\t\"wizardpen\"\n"
		"\tOption\t\t\"Device\"\t\"%s\"\n"
		"\tOption\t\t\"TopX\"\t\t\"%d\"\n"
		"\tOption\t\t\"TopY\"\t\t\"%d\"\n"
		"\tOption\t\t\"BottomX\"\t\"%d\"\n"
		"\tOption\t\t\"BottomY\"\t\"%d\"\n"
		"\tOption\t\t\"MaxX\"\t\t\"%d\"\n"
		"\tOption\t\t\"MaxY\"\t\t\"%d\"\n",
		filename,
		TopX, TopY,
		BottomX, BottomY,
		BottomX, BottomY
	);

	close_input( input );
	exit( 0 );
}

