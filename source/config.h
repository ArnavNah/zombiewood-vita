#ifndef CONFIG_H
#define CONFIG_H

#define VITA_TITLEID "ZOMBW0001"
#define VITA_APP_NAME "Zombiewood"

#define SCREEN_W 960
#define SCREEN_H 544

#ifndef DATA_PATH
#define DATA_PATH "ux0:data/zombiewood/"
#endif
#ifndef SO_PATH
#define SO_PATH DATA_PATH "libZombieWood.so"
#endif
#define OBB_MAIN_PATH DATA_PATH "main.1501.com.gameloft.android.ANMP.GloftZOHM.obb"
#define OBB_PATCH_PATH DATA_PATH "patch.15325.com.gameloft.android.ANMP.GloftZOHM.obb"

#define PACKAGE_NAME "com.gameloft.android.ANMP.GloftZOHM"

#define DEFAULT_FPS 60

#endif // CONFIG_H
