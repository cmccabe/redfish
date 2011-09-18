/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/test.h"

#include <stdlib.h>

void die_unless(int t)
{
  if (!t)
    abort();
}

void die_if(int t)
{
  if (t)
    abort();
}
