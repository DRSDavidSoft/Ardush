

char ch;
byte cursorPos = 0;

const byte CMD_LEN = 8;
char command[CMD_LEN];

bool escFlag = false;
char escMode;

#define KEY_BELL   0x07
#define KEY_ESCAPE 0x1B

#define KEY_TAB        0x09
#define KEY_ENTER      0x0D
#define KEY_BACKSPACE  0x7F

#define ARR_UP    65
#define ARR_DOWN  66
#define ARR_RIGHT 67
#define ARR_LEFT  68

void moveCursor( char direction ) {
	
	if ( direction > 0 ) {
		Serial.write( KEY_ESCAPE );
		Serial.write( "[" );
		Serial.write( direction );
	}
}

void newPrompt() {
	Serial.print("> ");
	
	memset(&command[0], 0, sizeof(command)); 
	cursorPos = 0;
}

void execPrompt() {
	Serial.println("Executing '"+String(command)+"' command.\n");
	newPrompt();
}

void setup() {
	Serial.begin(9600);

	newPrompt();
}

void loop() {

	if ( Serial.available() > 0 ) {

		ch = Serial.read();
		//Serial.println( (int) ch ); return;
		
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
			
			// We have a complete escape command
			else {
				
				// Do something with it
				if ( escMode == '[' ) {
					
					if ( ch == ARR_LEFT ) {
						if ( cursorPos > 0 ) {
							cursorPos--;
							moveCursor(ch);
						} else
						Serial.write( KEY_BELL );
					} else
					
					if ( ch == ARR_RIGHT ) {
						if ( cursorPos < strlen(command) ) {
							cursorPos++;
							moveCursor(ch);
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
				if ( strlen(command) > 0 ) execPrompt();
				else newPrompt();
			} else
			
			// Handle Backspace
			if ( ch == KEY_BACKSPACE ) {
				if (cursorPos > 0 ) {
					command[cursorPos--] = '\0';
					Serial.write( KEY_BACKSPACE );
				} else
				Serial.write( KEY_BELL );
			} else
			
			// Handle all other keys
			{
				if ( cursorPos < CMD_LEN ) {
					command[cursorPos++] = ch;
					Serial.write( ch );
				} else
				Serial.write( KEY_BELL );
			}

		}
		
		
	}

	
}
