#ifndef UAMS_HEADER
#define UAMS_HEADER

struct uam_static_mod {
    char *name;
    struct uam_export *def;
};

#define UAMS_STATIC_MODS_COUNT  4
extern struct uam_static_mod uam_static_mod_0;
extern struct uam_static_mod uam_static_mod_1;
extern struct uam_static_mod uam_static_mod_2;
extern struct uam_static_mod uam_static_mod_3;

#define UAM_STATIC_MOD(I, N, S) \
        struct uam_static_mod _MOD_VAR(I) = { \
            N, S \
        };

#define _MOD_VAR(I) uam_static_mod_ ## I

#define UAMS_DHX        0
#define UAMS_DHX_PAM    1
#define UAMS_DHX2       2
#define UAMS_DHX2_PAM   3

#define uam_static_mod_get(N) ( \
        !strcmp(N, uam_static_mod_0.name) ? uam_static_mod_0.def : ( \
            !strcmp(N, uam_static_mod_1.name) ? uam_static_mod_1.def : ( \
                !strcmp(N, uam_static_mod_2.name) ? uam_static_mod_2.def : ( \
                    !strcmp(N, uam_static_mod_3.name) ? uam_static_mod_3.def : ( \
                        NULL \
                    ) \
                ) \
            ) \
        ) \
    )

#endif
