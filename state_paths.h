#pragma once
// READ ME: single source of truth for packaged/shared state locations.
// GUI and daemon must resolve compatible state paths, especially for Snap.

// #ju56Us

#include <QString>

namespace StatePaths
{
QString sharedStateDir();
QString packagingMode();
QString stateExplanation();
}
