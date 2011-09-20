/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_MON_SSH_DOT_H
#define ONEFISH_MON_SSH_DOT_H

#include <unistd.h> /* for size_t */

/* The monitor uses ssh to start daemons on remote nodes, kill daemons on
 * remote nodes, and generally administer the cluster.
 * Once the daemons are started, we can obviously communicate with them over
 * the network without using ssh.
 */

#define ONEFISH_SSH_ERR 0x100
#define ONEFISH_SSH_ARG_ERR 0x101

/** Run an ssh command on a remote machine.
 *
 * Because ssh runs commands through the shell, shell expansion will be
 * performed on the parameters you pass here, so be careful! Thankfully, you
 * will only need to worry about one round of shell expansion, though.
 * You can't read more than 512 bytes of stderr or stdout using this command
 * because of implementation details.
 *
 * This is a blocking call.
 *
 * @param host			The host to connect to.
 * @param out			The output buffer to write stderr / stdout to.
 * @param out_len		The length of the out buffer.
 * @param cmd		        The command as a NULL-terminated array of
 *				strings
 *
 * Returns the error code of the process, or a special ONEFISH_SSH error code.
 */
int ssh_exec(const char *host, char *out, size_t out_len, const char **cmd);

#endif
