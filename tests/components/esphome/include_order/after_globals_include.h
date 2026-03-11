#pragma once

#ifndef DEFINED_FIRST
#error "before-globals include was not emitted before generated globals"
#else
#define DEFINED_SECOND DEFINED_FIRST
#endif

static_assert(sizeof(int_global) > 0, "late include could not see int_global");
static_assert(sizeof(array_global) > 0, "late include could not see array_global");
static_assert(sizeof(custom_type_global) > 0, "late include could not see custom_type_global");
