/*
 * Copyright 2012 the Redfish authors
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

#ifndef REDFISH_CORE_ENV_DOT_H
#define REDFISH_CORE_ENV_DOT_H

/** Get the value of an environment variable or die
 *
 * @param key		The name of the environment variable
 *
 * @return		the value of the environment variable
 */
extern const char *getenv_or_die(const char *key);

/** Get the value of an environment variable or return a default
 *
 * @param key		The name of the environment variable
 * @param def		The default value
 *
 * @return		the value of the environment variable, or def if the
 *			variable is not set.
 */
extern const char *getenv_with_default(const char *key, const char *def);

#endif
