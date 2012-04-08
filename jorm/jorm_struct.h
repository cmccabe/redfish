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

#define JORM_CONTAINER_BEGIN(name) struct name {
#define JORM_INT(name) int name;
#define JORM_DOUBLE(name) double name;
#define JORM_STR(name) char *name;
#define JORM_NESTED(name, ty) struct ty *name;
#define JORM_EMBEDDED(name, ty) struct ty *name;
#define JORM_BOOL(name) int name;
#define JORM_SARRAY(name) char **name;
#define JORM_OARRAY(name, ty) struct ty **name;
#define JORM_CONTAINER_END };
#define JORM_IGNORE(x) x
