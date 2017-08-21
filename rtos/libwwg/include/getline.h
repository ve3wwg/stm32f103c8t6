/* Get an edited line
 * Warren W. Gay VE3WWG
 */
#ifndef GETLINE_H
#define GETLINE_H

#include <stdint.h>

int getline(char *buf,unsigned bufsiz,int (*getc)(void),void (*putc)(char ch));

#endif // GETLINE_H

// End getline
