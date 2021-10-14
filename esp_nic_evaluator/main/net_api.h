#ifndef __NET_API__
#define __NET_API__


/**
 * net_sock() - Creates a socket.
 *
 * Aquires a socket in the socket table and associates it with the process.
 */
int net_sock(void);

/**
 * net_bind() - Bind a port to a socket.
 *
 * @sock    socket that receives data for that port
 * @port    which network port to bind socket to
 *
 * Bind a port to a socket associated with the process. The socket may be
 * used if the return value is 0.
 */
int net_bind(int sock, unsigned short port);

/**
 * net_recv() - Receive data from port.
 *
 * @sock    socket that receives data for that port
 * @size    how much data to be received
 *
 * Receives `size` count of bytes blockingly. The function installs a
 * callback in the socket table which is executed if a packet is received
 * on that port.
 */
int net_recv(int sock, void* dest, int size);


#endif
