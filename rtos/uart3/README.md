DEMO UART3
----------

This is a UART demo using library libwwg/src/uartlib.c, and of FreeRTOS
queues. Task demo_task() continually sends messages to the terminal
(through the UART), through the FreeRTOS queue (using uart_puts()). The
application uart_puts() function queues bytes to the uart_task() to be
received and then displayed.

The uart_task() looks at queue uart_txq with a timeout of 10 ms. If there
is a message received (a byte to be sent through the UART), it is
transmitted using the putc_uart() library call.

When the queue receive function times out, it checks at the top of the
loop if there is any input data available using getc_uart_nb(). This
non-blocking call returns -1 if there is no input, but otherwise returns
the ASCII char that was received. If the received character is a CR or
LF, uart_task() then prompts for further input and obtains it using
getline_uart(). If the character received was not CR/LF, it saves that
as the first input character and prompts for the rest of the line.

Once the user has typed CR (or LF), the line is terminated and returned
to the uart_task(). This in turn reports the input line received.

SAMPLE SESSION
--------------

    Now this is a message..
      sent via FreeRTOS queues.
    
    Just start typing to enter a line, or..
    hit Enter first, then enter your input.
    
    
    
    ENTER INPUT: yes master?
    Received input 'yes master?'
    
    Resuming prints...
    Now this is a message..
      sent via FreeRTOS queues.

BACKSPACE NOTE:
---------------

Note that while getline_uart() allows the user to backspace and edit
their input, you cannot backspace over the first character, if your
first character was not CR or LF. If you force it to prompt first, you
will then be able to edit the entire line because the entire input line
will be under the control of getline_uart().
