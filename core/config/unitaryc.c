/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/config/logc.h"
#include "core/config/mdsc.h"
#include "core/config/osdc.h"
#include "core/config/unitaryc.h"

#define JORM_CUR_FILE "core/config/unitaryc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE
