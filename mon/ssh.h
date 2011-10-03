/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MON_SSH_DOT_H
#define REDFISH_MON_SSH_DOT_H

#include <unistd.h> /* for size_t */

/* The monitor uses ssh to start daemons on remote nodes, kill daemons on
 * remote nodes, and generally administer the cluster.
 * Once the daemons are started, we can obviously communicate with them over
 * the network without using ssh.
 */

#define REDFISH_SSH_ERR 0x100
#define REDFISH_SSH_ARG_ERR 0x101

/** Run an ssh command on a remote machine.
 *
 * Because ssh runs commands through the shell, shell expansion will be
 * performed on the parameters you pass here, so be careful! Thankfully, you
 * will only need to worry about one round of shell expansion, though.
 * You can't read more than 512 bytes of stderr or stdout using this command
 * because of implementation details.
 *
 * This blocks until ssh finishes running the command.
 *
 * @param host		The host to connect to.
 * @param out		The output buffer to write stderr / stdout to.
 * @param out_len	The length of the out buffer.
 * @param cmd	        The command as a NULL-terminated array of strings
 *
 * Returns the error code of the process, or a special REDFISH_SSH error code.
 */
int ssh_exec(const char *host, char *out, size_t out_len, const char **cmd);

/** Start an ssh command that connects to a remote machine.
 * Returns a file descriptor you can use to provide input to that remote
 * machine.
 *
 * Remember:
 * - Parameters will be shell-expanded
 * - Just because this succeeds, doesn't mean your command will succeed! Check
 *   do_waitpid.
 * - This blocks until ssh connects.
 *
 * @param host		The host to connect to.
 * @param cmd		The command as a NULL-terminated array of strings
 * @param pid		(out param) The process ID of the ssh process that we
 *			have started, if process creation succeeds.
 *
 * @return		A negative error code if ssh could not be started, or a
 *			file descriptor to write to
 */
int start_ssh_give_input(const char *host, const char **cmd, int *pid);

#endif
