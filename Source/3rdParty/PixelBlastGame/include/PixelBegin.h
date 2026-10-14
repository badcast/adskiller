#pragma once
#ifdef PB_SHARED
#define PB_EXPORT Q_DECL_EXPORT
#else
#define PB_EXPORT Q_DECL_IMPORT
#endif

struct PixelStats;
class SoundManager;
class PixelNetwork;
