

unsigned char ch;
byte cursorPos = 0;

const unsigned int CMD_LEN = 32;
char command[CMD_LEN + 1];

bool escFlag = false;
char escMode;

#define KEY_BKSP   0x08
#define KEY_BELL   0x07
#define KEY_ESCAPE 0x1B

#define KEY_TAB        0x09
#define KEY_RETURN     0x0A
#define KEY_ENTER      0x0D
#define KEY_BACKSPACE  0x7F

#define ARR_UP    65
#define ARR_DOWN  66
#define ARR_RIGHT 67
#define ARR_LEFT  68

void sendCursor( unsigned char direction ) {
	if ( direction > 0 ) {
		Serial.write( KEY_ESCAPE );
		Serial.write( "[" );
		Serial.write( direction );
	}
}

void shiftToLeft() {
	for (int i = cursorPos; i<CMD_LEN; i++ ) {
		command[i] = command[i+1];
	}
}

void shiftToRight() {
	for (int i = CMD_LEN; i>cursorPos; i-- ) {
		command[i] = command[i-1];
	}
}

void fixPos() {
	
	if ( cursorPos != strlen(command) ) {
		sendCursor('s'); // Tell the terminal to keep the current position
		for (int i = cursorPos; i<strlen(command); i++)
			Serial.write( command[i] );
		Serial.write(32);
		sendCursor('u'); // Tell the terminal to restore the position
	}
	
}

void newPrompt() {
	Serial.print("> ");
	
	memset(&command[0], 0, sizeof(command)); 
	cursorPos = 0;
}

void execPrompt() {
	Serial.println("Executing '"+String(command)+"' command.\n");

	if ( strcmp(command, "clear") == 0 ) {

		/** Put cursor to home */
		Serial.write( KEY_ESCAPE );
		Serial.write( "[H" );

		/** Clear screen */
		Serial.write( KEY_ESCAPE );
		Serial.write( "[2J" );

	} else

	if ( strcmp(command, "test") == 0 ) {

		/** Send test code */
		//Serial.write( KEY_ESCAPE );
		//Serial.write( "[Z" );
		Serial.write( 5 ); // CTRL+E

	} else	

	if ( strcmp(command, "exit") == 0 ) {
		Serial.write( KEY_ESCAPE );
		Serial.write( "[2J" );
	} else

	if ( strcmp(command, "keycodes") == 0 ) {

		Serial.println("Keycodes debugging. Press CTRL+C to exit.");

		while (true) {

			if ( Serial.available() > 0 ) {
				ch = Serial.read();

				if ( ch == 0x03 ) break;
				else Serial.println( (int) ch ); 
			}

		}

	}

	else
	Serial.println("Command not found.");

}

void setup() {
	Serial.begin(9600);

	newPrompt();
}

void loop() {

	if ( Serial.available() > 0 ) {

		/** Read the current character */
		ch = Serial.read();
		
		/** If we are currently processing an escape sequence */
		if ( escFlag == true ) {
			
			// Which escape command are we decoding
			if ( escMode == 0 ) {
				switch (ch) {
					case '[':
						escMode = ch;
						break;
					default:
						escFlag = false;
						escMode = 0;
						// Received escape sequence is invalid
						Serial.write( KEY_BELL );
						break;
				}
			}
			
			// Parse escape sequence attributes
			else {
				
				// We have a complete escape command
				if ( escMode == '[' ) {
					
					if ( ch == ARR_LEFT ) {
						if ( cursorPos > 0 ) {
							cursorPos--;
							sendCursor(ch);
						} else
						Serial.write( KEY_BELL );
					} else
					
					if ( ch == ARR_RIGHT ) {
						if ( cursorPos < strlen(command) ) {
							cursorPos++;
							sendCursor(ch);
						} else
						Serial.write( KEY_BELL );
					} else
					
					{
						// Received escape sequence is invalid
						Serial.write( KEY_BELL );
					}
					
				}
				
				// Reset the escape sequence
				escFlag = false;
				escMode = 0;
				
			}
			
		}
		
		else {
			
			// Handle Escape Key sequence
			if ( ch == KEY_ESCAPE ) {
				escFlag = true;
				escMode = 0;
			} else
				
			// Handle CTRL-C key code
			if ( ch == 0x03 ) {
				Serial.println();
				newPrompt();
			} else
				
			// Handle Tab
			if ( ch == KEY_TAB ) {
				// Not supported yet
				Serial.write( KEY_BELL );
			} else

			// Handle New line
			if ( ch == KEY_ENTER ) {
				Serial.println();
				if ( strlen(command) > 0 )
					execPrompt();
				newPrompt();
			} else

			// Handle Carriage Return
			if ( ch == KEY_RETURN ) {
				// Do nothing; the "\r" character is useless
			} else
			
			// Handle Backspace
			if ( ch == KEY_BKSP || ch == KEY_BACKSPACE ) {
				if (cursorPos > 0 ) {
					cursorPos--;
					
					shiftToLeft();
					Serial.write( KEY_BACKSPACE );
					
					fixPos();
				} else
				Serial.write( KEY_BELL );
			} else
			
			// Handle all other keys
			{
				if ( strlen(command) < CMD_LEN ) {
					shiftToRight();
					
					command[cursorPos++] = ch;
					Serial.write( ch );
					
					fixPos();
				} else
				Serial.write( KEY_BELL );
			}

		}
		
		
	}

	
}
