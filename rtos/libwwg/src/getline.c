/* Get an edited line
 * Warren W. Gay VE3WWG
 */
#include <getline.h>
#include <memory.h>

#define CONTROL(c) ((c) & 0x1F)

/*********************************************************************
 * A very simple line editing routine. It supports:
 *
 *	^U	Kill line
 *	^A	Begin line
 *	^B	Backup character
 *	^F	Forward character
 *	^E	End line
 *	^H	Backspace
 *	^I	Insert a blank
 *	^D or rubout
 *		Delete char
 *
 * The returned line is NOT terminated with '\n' like fgets(), but 
 * rather works like the old gets().
 *
 * Returns the number of characters returned in buf.
 *
 *********************************************************************/

int
getline(char *buf,unsigned bufsiz,int (*get)(void),void (*put)(char ch)) {
	char ch = 0;
	unsigned bufx = 0, buflen = 0;

	if ( bufsiz <= 1 )
		return -1;
	--bufsiz;		// Leave room for nul byte

	while ( ch != '\n' ) {
		ch = get();

		switch ( ch ) {
		case CONTROL('U'):	// Kill line
			for ( ; bufx > 0; --bufx )
				put('\b');
			for ( ; bufx < buflen; ++bufx )
				put(' ');
			buflen = 0;
			// Fall thru
		case CONTROL('A'):	// Begin line
			for ( ; bufx > 0; --bufx )
				put('\b');
			break;
		case CONTROL('B'):	// Backward char
			if ( bufx > 0 ) {
				--bufx;
				put('\b');
			}
			break;
		case CONTROL('F'):	// Forward char
			if ( bufx < bufsiz && bufx < buflen )
				put(buf[++bufx]);
			break;
		case CONTROL('E'):	// End line
			for ( ; bufx < buflen; ++bufx )
				put(buf[bufx]);
			break;
		case CONTROL('H'):	// Backspace char
		case 0x7F:		// Rubout
			if ( bufx <= 0 )
				break;
			--bufx;
			put('\b');
			// Fall thru
		case CONTROL('D'):	// Delete char
			if ( bufx < buflen ) {
				memmove(buf+bufx,buf+bufx+1,buflen-bufx-1);
				--buflen;
				for ( unsigned x=bufx; x<buflen; ++x )
					put(buf[x]);
				put(' ');
				for ( unsigned x=buflen+1; x>bufx; --x )
					put('\b');
			}
			break;
		case CONTROL('I'):	// Insert characters (TAB)
			if ( bufx < buflen && buflen + 1 < bufsiz ) {
				memmove(buf+bufx+1,buf+bufx,buflen-bufx);
				buf[bufx] = ' ';
				++buflen;
				put(' ');
				for ( unsigned x=bufx+1; x<buflen; ++x )
					put(buf[x]);
				for ( unsigned x=bufx; x<buflen; ++x )
					put('\b');
			}
			break;
		case '\r':
		case '\n':		// End line
			ch = '\n';
			break;
		default:		// Overtype
			if ( bufx >= bufsiz ) {
				put(0x07);	// Bell
				continue;	// No room left
			}
			buf[bufx++] = ch;
			put(ch);
			if ( bufx > buflen )
				buflen = bufx;
		}

		if ( bufx > buflen )
			buflen = bufx;
	}

	buf[buflen] = 0;
	put('\n');
	put('\r');
	return bufx;
}

// End getline
