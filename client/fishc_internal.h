/*
 * Copyright 2011-2012 the Redfish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef REDFISH_CLIENT_FISHC_IMPL_DOT_H
#define REDFISH_CLIENT_FISHC_IMPL_DOT_H

/** Default MDS port */
#define REDFISH_DEFAULT_MDS_PORT 9000

/** Redfish replication count
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_REPL 3

/** Redfish fixed 64 MB local buffer size
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_LBUF_SZ 67108864

/** Redfish fixed 64 MB chunk size
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_BLOCK_SZ 67108864

/** Default mode for files */
#define REDFISH_DEFAULT_FILE_MODE 0644

/** Default mode for files */
#define REDFISH_DEFAULT_DIR_MODE 0755

#endif
