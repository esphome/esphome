#pragma once

// Helper macro to define a version code, whose value can be compared against other version codes.
#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))
