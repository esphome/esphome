#pragma once

// Helper macro to define a version code, whos evalue can be compared against other version codes.
#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))
