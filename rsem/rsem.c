/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "common/config/logc.h"
#include "rsem/rsem.h"

#define JORM_CUR_FILE "rsem/rsem.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE
