/* Get an edited line
 * Warren W. Gay VE3WWG
 */
#ifndef GETLINE_H
#define GETLINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int getline(char *buf,unsigned bufsiz,int (*getc)(void),void (*putc)(char ch));

#ifdef __cplusplus
}
#endif

#endif // GETLINE_H

// End getline
