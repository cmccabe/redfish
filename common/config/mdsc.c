/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "common/config/logc.h"
#include "common/config/mdsc.h"
#include "common/config/mstorc.h"

#define JORM_CUR_FILE "common/config/mdsc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE
