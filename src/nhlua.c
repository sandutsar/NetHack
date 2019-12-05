/* NetHack 3.7	nhlua.c	$NHDT-Date: 1575246766 2019/12/02 00:32:46 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.16 $ */
/*      Copyright (c) 2018 by Pasi Kallinen */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "dlb.h"

/*
#- include <lua5.3/lua.h>
#- include <lua5.3/lualib.h>
#- include <lua5.3/lauxlib.h>
*/

/*  */

/* lua_CFunction prototypes */
static int FDECL(nhl_test, (lua_State *));
static int FDECL(nhl_getmap, (lua_State *));
#if 0
static int FDECL(nhl_setmap, (lua_State *));
#endif
static int FDECL(nhl_pline, (lua_State *));
static int FDECL(nhl_verbalize, (lua_State *));
static int FDECL(nhl_menu, (lua_State *));
static int FDECL(nhl_getlin, (lua_State *));
static int FDECL(nhl_makeplural, (lua_State *));
static int FDECL(nhl_makesingular, (lua_State *));
static int FDECL(nhl_s_suffix, (lua_State *));
static int FDECL(nhl_ing_suffix, (lua_State *));
static int FDECL(nhl_an, (lua_State *));
static int FDECL(nhl_meta_u_index, (lua_State *));
static int FDECL(nhl_meta_u_newindex, (lua_State *));
static int FDECL(traceback_handler, (lua_State *));

void
nhl_error(L, msg)
lua_State *L;
const char *msg;
{
    lua_Debug ar;
    char buf[BUFSZ];

    lua_getstack(L, 1, &ar);
    lua_getinfo(L, "lS", &ar);
    Sprintf(buf, "%s (line %i%s)", msg, ar.currentline, ar.source);
    lua_pushstring(L, buf);
    (void) lua_error(L);
    /*NOTREACHED*/
}

/* Check that parameters are nothing but single table,
   or if no parameters given, put empty table there */
void
lcheck_param_table(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc < 1)
        lua_createtable(L, 0, 0);

    /* discard any extra arguments passed in */
    lua_settop(L, 1);

    luaL_checktype(L, 1, LUA_TTABLE);
}

schar
get_table_mapchr(L, name)
lua_State *L;
const char *name;
{
    char *ter;
    xchar typ;

    ter = get_table_str(L, name);
    typ = check_mapchr(ter);
    if (typ == INVALID_TYPE)
        nhl_error(L, "Erroneous map char");
    if (ter)
        free(ter);
    return typ;
}

schar
get_table_mapchr_opt(L, name, defval)
lua_State *L;
const char *name;
schar defval;
{
    char *ter;
    xchar typ;

    ter = get_table_str_opt(L, name, emptystr);
    if (name && *ter) {
        typ = check_mapchr(ter);
        if (typ == INVALID_TYPE)
            nhl_error(L, "Erroneous map char");
    } else
        typ = defval;
    if (ter)
        free(ter);
    return typ;
}

void
nhl_add_table_entry_int(L, name, value)
lua_State *L;
const char *name;
int value;
{
    lua_pushstring(L, name);
    lua_pushinteger(L, value);
    lua_rawset(L, -3);
}

void
nhl_add_table_entry_str(L, name, value)
lua_State *L;
const char *name;
const char *value;
{
    lua_pushstring(L, name);
    lua_pushstring(L, value);
    lua_rawset(L, -3);
}
void
nhl_add_table_entry_bool(L, name, value)
lua_State *L;
const char *name;
boolean value;
{
    lua_pushstring(L, name);
    lua_pushboolean(L, value);
    lua_rawset(L, -3);
}

/* converting from special level "map character" to levl location type
   and back. order here is important. */
const struct {
    char ch;
    schar typ;
} char2typ[] = {
                { ' ', STONE },
                { '#', CORR },
                { '.', ROOM },
                { '-', HWALL },
                { '-', TLCORNER },
                { '-', TRCORNER },
                { '-', BLCORNER },
                { '-', BRCORNER },
                { '-', CROSSWALL },
                { '-', TUWALL },
                { '-', TDWALL },
                { '-', TLWALL },
                { '-', TRWALL },
                { '-', DBWALL },
                { '|', VWALL },
                { '+', DOOR },
                { 'A', AIR },
                { 'C', CLOUD },
                { 'S', SDOOR },
                { 'H', SCORR },
                { '{', FOUNTAIN },
                { '\\', THRONE },
                { 'K', SINK },
                { '}', MOAT },
                { 'P', POOL },
                { 'L', LAVAPOOL },
                { 'I', ICE },
                { 'W', WATER },
                { 'T', TREE },
                { 'F', IRONBARS }, /* Fe = iron */
                { 'x', MAX_TYPE }, /* "see-through" */
                { 'B', CROSSWALL }, /* hack: boundary location */
                { '\0', STONE },
};

schar
splev_chr2typ(c)
char c;
{
    int i;

    for (i = 0; char2typ[i].ch; i++)
        if (c == char2typ[i].ch)
            return char2typ[i].typ;
    return (INVALID_TYPE);
}

schar
check_mapchr(s)
const char *s;
{
    if (s && strlen(s) == 1)
        return splev_chr2typ(s[0]);
    return INVALID_TYPE;
}

char
splev_typ2chr(typ)
schar typ;
{
    int i;

    for (i = 0; char2typ[i].typ < MAX_TYPE; i++)
        if (typ == char2typ[i].typ)
            return char2typ[i].ch;
    return 'x';
}

/* local loc = getmap(x,y) */
static int
nhl_getmap(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 2) {
        int x = (int) lua_tointeger(L, 1);
        int y = (int) lua_tointeger(L, 2);

        if (x >= 0 && x < COLNO && y >= 0 && y < ROWNO) {
            char buf[BUFSZ];
            lua_newtable(L);

            /* FIXME: some should be boolean values */
            nhl_add_table_entry_int(L, "glyph", levl[x][y].glyph);
            nhl_add_table_entry_int(L, "typ", levl[x][y].typ);
            Sprintf(buf, "%c", splev_typ2chr(levl[x][y].typ));
            nhl_add_table_entry_str(L, "mapchr", buf);
            nhl_add_table_entry_int(L, "seenv", levl[x][y].seenv);
            nhl_add_table_entry_bool(L, "horizontal", levl[x][y].horizontal);
            nhl_add_table_entry_bool(L, "lit", levl[x][y].lit);
            nhl_add_table_entry_bool(L, "waslit", levl[x][y].waslit);
            nhl_add_table_entry_int(L, "roomno", levl[x][y].roomno);
            nhl_add_table_entry_bool(L, "edge", levl[x][y].edge);
            nhl_add_table_entry_bool(L, "candig", levl[x][y].candig);

            /* TODO: FIXME: levl[x][y].flags */

            lua_pushliteral(L, "flags");
            lua_newtable(L);

            if (IS_DOOR(levl[x][y].typ)) {
                nhl_add_table_entry_bool(L, "nodoor", (levl[x][y].flags & D_NODOOR));
                nhl_add_table_entry_bool(L, "broken", (levl[x][y].flags & D_BROKEN));
                nhl_add_table_entry_bool(L, "isopen", (levl[x][y].flags & D_ISOPEN));
                nhl_add_table_entry_bool(L, "closed", (levl[x][y].flags & D_CLOSED));
                nhl_add_table_entry_bool(L, "locked", (levl[x][y].flags & D_LOCKED));
                nhl_add_table_entry_bool(L, "trapped", (levl[x][y].flags & D_TRAPPED));
            } else if (IS_ALTAR(levl[x][y].typ)) {
                /* TODO: bits 0, 1, 2 */
                nhl_add_table_entry_bool(L, "shrine", (levl[x][y].flags & AM_SHRINE));
            } else if (IS_THRONE(levl[x][y].typ)) {
                nhl_add_table_entry_bool(L, "looted", (levl[x][y].flags & T_LOOTED));
            } else if (levl[x][y].typ == TREE) {
                nhl_add_table_entry_bool(L, "looted", (levl[x][y].flags & TREE_LOOTED));
                nhl_add_table_entry_bool(L, "swarm", (levl[x][y].flags & TREE_SWARM));
            } else if (IS_FOUNTAIN(levl[x][y].typ)) {
                nhl_add_table_entry_bool(L, "looted", (levl[x][y].flags & F_LOOTED));
                nhl_add_table_entry_bool(L, "warned", (levl[x][y].flags & F_WARNED));
            } else if (IS_SINK(levl[x][y].typ)) {
                nhl_add_table_entry_bool(L, "pudding", (levl[x][y].flags & S_LPUDDING));
                nhl_add_table_entry_bool(L, "dishwasher", (levl[x][y].flags & S_LDWASHER));
                nhl_add_table_entry_bool(L, "ring", (levl[x][y].flags & S_LRING));
            }
            /* TODO: drawbridges, walls, ladders, room=>ICED_xxx */

            lua_settable(L, -3);

            return 1;
        } else {
            /* TODO: return zerorm instead? */
            nhl_error(L, "Coordinates out of range");
            return 0;
        }
    } else {
        nhl_error(L, "Incorrect arguments");
        return 0;
    }
    return 1;
}

/* pline("It hits!") */
static int
nhl_pline(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1)
        pline("%s", luaL_checkstring(L, 1));
    else
        nhl_error(L, "Wrong args");

    return 0;
}

/* verbalize("Fool!") */
static int
nhl_verbalize(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1)
        verbalize("%s", luaL_checkstring(L, 1));
    else
        nhl_error(L, "Wrong args");

    return 0;
}

/*
  str = getlin("What do you want to call this dungeon level?");
 */
static int
nhl_getlin(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1) {
        const char *prompt = luaL_checkstring(L, 1);
        char buf[BUFSZ];

        getlin(prompt, buf);
        lua_pushstring(L, buf);
        return 1;
    }

    nhl_error(L, "Wrong args");
    return 0;
}

/*
 selected = menu("prompt", default, pickX, { "a" = "option a", "b" = "option b", ...})
 pickX = 0,1,2, or "none", "one", "any" (PICK_X in code)

 selected = menu("prompt", default, pickX,
                { {key:"a", text:"option a"}, {key:"b", text:"option b"}, ... } ) */
static int
nhl_menu(L)
lua_State *L;
{
    int argc = lua_gettop(L);
    const char *prompt;
    const char *defval = "";
    const char *const pickX[] = {"none", "one", "any"}; /* PICK_NONE, PICK_ONE, PICK_ANY */
    int pick = PICK_ONE, pick_cnt;
    winid tmpwin;
    anything any;
    menu_item *picks = (menu_item *) 0;

    if (argc < 2 || argc > 4) {
        nhl_error(L, "Wrong args");
        return 0;
    }

    prompt = luaL_checkstring(L, 1);
    if (lua_isstring(L, 2))
        defval = luaL_checkstring(L, 2);
    if (lua_isstring(L, 3))
        pick = luaL_checkoption(L, 3, "one", pickX);
    luaL_checktype(L, argc, LUA_TTABLE);

    tmpwin = create_nhwindow(NHW_MENU);
    start_menu(tmpwin);

    lua_pushnil(L); /* first key */
    while (lua_next(L, argc) != 0) {
        const char *str = "";
        const char *key = "";

        /* key @ index -2, value @ index -1 */
        if (lua_istable(L, -1)) {
            lua_pushliteral(L, "key");
            lua_gettable(L, -2);
            key = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_pushliteral(L, "text");
            lua_gettable(L, -2);
            str = lua_tostring(L, -1);
            lua_pop(L, 1);

            /* TODO: glyph, attr, accel, group accel (all optional) */
        } else if (lua_isstring(L, -1)) {
            str = luaL_checkstring(L, -1);
            key = luaL_checkstring(L, -2);
        }

        any = cg.zeroany;
        if (*key)
            any.a_char = key[0];
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, str,
                 (*defval && *key && defval[0] == key[0]) ? MENU_SELECTED : MENU_UNSELECTED);

        lua_pop(L, 1); /* removes 'value'; keeps 'key' for next iteration */
    }

    end_menu(tmpwin, prompt);
    pick_cnt = select_menu(tmpwin, pick, &picks);
    destroy_nhwindow(tmpwin);

    if (pick_cnt > 0) {
        char buf[2];
        buf[0] = picks[0].item.a_char;

        if (pick == PICK_ONE && pick_cnt > 1 && *defval && defval[0] == picks[0].item.a_char)
            buf[0] = picks[1].item.a_char;

        buf[1] = '\0';
        lua_pushstring(L, buf);
        /* TODO: pick any */
    } else {
        char buf[2];
        buf[0] = defval[0];
        buf[1] = '\0';
        lua_pushstring(L, buf);
    }

    return 1;
}

/* makeplural("zorkmid") */
static int
nhl_makeplural(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1)
        lua_pushstring(L, makeplural(luaL_checkstring(L, 1)));
    else
        nhl_error(L, "Wrong args");

    return 1;
}

/* makesingular("zorkmids") */
static int
nhl_makesingular(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1)
        lua_pushstring(L, makesingular(luaL_checkstring(L, 1)));
    else
        nhl_error(L, "Wrong args");

    return 1;
}

/* s_suffix("foo") */
static int
nhl_s_suffix(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1)
        lua_pushstring(L, s_suffix(luaL_checkstring(L, 1)));
    else
        nhl_error(L, "Wrong args");

    return 1;
}

/* ing_suffix("foo") */
static int
nhl_ing_suffix(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1)
        lua_pushstring(L, ing_suffix(luaL_checkstring(L, 1)));
    else
        nhl_error(L, "Wrong args");

    return 1;
}

/* an("foo") */
static int
nhl_an(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1)
        lua_pushstring(L, an(luaL_checkstring(L, 1)));
    else
        nhl_error(L, "Wrong args");

    return 1;
}

/* get mandatory integer value from table */
int
get_table_int(L, name)
lua_State *L;
const char *name;
{
    int ret;

    lua_getfield(L, -1, name);
    ret = (int) luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    return ret;
}

/* get optional integer value from table */
int
get_table_int_opt(L, name, defval)
lua_State *L;
const char *name;
int defval;
{
    int ret = defval;

    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1)) {
        ret = (int) luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
    return ret;
}

char *
get_table_str(L, name)
lua_State *L;
const char *name;
{
    char *ret;

    lua_getfield(L, -1, name);
    ret = dupstr(luaL_checkstring(L, -1));
    lua_pop(L, 1);
    return ret;
}

/* get optional string value from table.
   return value must be freed by caller. */
char *
get_table_str_opt(L, name, defval)
lua_State *L;
const char *name;
char *defval;
{
    const char *ret;

    lua_getfield(L, -1, name);
    ret = luaL_optstring(L, -1, defval);
    if (ret) {
        lua_pop(L, 1);
        return dupstr(ret);
    }
    lua_pop(L, 1);
    return NULL;
}

int
get_table_boolean(L, name)
lua_State *L;
const char *name;
{
    int ltyp;
    int ret = -1;

    lua_getfield(L, -1, name);
    ltyp = lua_type(L, -1);
    if (ltyp == LUA_TSTRING) {
        const char *const boolstr[] = { "true", "false", "yes", "no", NULL };
        /* const int boolstr2i[] = { TRUE, FALSE, TRUE, FALSE, -1 }; */

        ret = luaL_checkoption(L, -1, NULL, boolstr);
        /* nhUse(boolstr2i[0]); */
    } else if (ltyp == LUA_TBOOLEAN) {
        ret = lua_toboolean(L, -1);
    } else if (ltyp == LUA_TNUMBER) {
        ret = (int) luaL_checkinteger(L, -1);
        if ( ret < 0 || ret > 1)
            ret = -1;
    }
    lua_pop(L, 1);
    if (ret == -1)
        nhl_error(L, "Expected a boolean");
    return ret;
}

int
get_table_boolean_opt(L, name, defval)
lua_State *L;
const char *name;
int defval;
{
    int ret = defval;

    lua_getfield(L, -1, name);
    if (lua_type(L, -1) != LUA_TNIL) {
        lua_pop(L, 1);
        return get_table_boolean(L, name);
    }
    lua_pop(L, 1);
    return ret;
}

int
get_table_option(L, name, defval, opts)
lua_State *L;
const char *name;
const char *defval;
const char *const opts[]; /* NULL-terminated list */
{
    int ret;

    lua_getfield(L, -1, name);
    ret = luaL_checkoption(L, -1, defval, opts);
    lua_pop(L, 1);
    return ret;
}

/*
  test( { x = 123, y = 456 } );
*/
static int
nhl_test(L)
lua_State *L;
{
    int x, y;
    char *name, Player[] = "Player";

    /* discard any extra arguments passed in */
    lua_settop(L, 1);

    luaL_checktype(L, 1, LUA_TTABLE);

    x = get_table_int(L, "x");
    y = get_table_int(L, "y");
    name = get_table_str_opt(L, "name", Player);

    pline("TEST:{ x=%i, y=%i, name=\"%s\" }", x,y, name);

    free(name);

    return 1;
}

static const struct luaL_Reg nhl_functions[] = {
    {"test", nhl_test},

    {"getmap", nhl_getmap},
#if 0
    {"setmap", nhl_setmap},
#endif
    {"pline", nhl_pline},
    {"verbalize", nhl_verbalize},
    {"menu", nhl_menu},
    {"getlin", nhl_getlin},

    {"makeplural", nhl_makeplural},
    {"makesingular", nhl_makesingular},
    {"s_suffix", nhl_s_suffix},
    {"ing_suffix", nhl_ing_suffix},
    {"an", nhl_an},
    {NULL, NULL}
};

static const struct {
    const char *name;
    long value;
} nhl_consts[] = {
    { "COLNO",  COLNO },
    { "ROWNO",  ROWNO },
    { NULL, 0 },
};

/* register and init the constants table */
void
init_nhc_data(L)
lua_State *L;
{
    int i;

    lua_newtable(L);

    for (i = 0; nhl_consts[i].name; i++) {
        lua_pushstring(L, nhl_consts[i].name);
        lua_pushinteger(L, nhl_consts[i].value);
        lua_rawset(L, -3);
    }

    lua_setglobal(L, "nhc");
}

int
nhl_push_anything(L, anytype, src)
lua_State *L;
int anytype;
void *src;
{
    anything any = cg.zeroany;
    switch (anytype) {
    case ANY_INT: any.a_int = *(int *)src; lua_pushinteger(L, any.a_int); break;
    case ANY_UCHAR: any.a_uchar = *(uchar *)src; lua_pushinteger(L, any.a_uchar); break;
    case ANY_SCHAR: any.a_schar = *(schar *)src; lua_pushinteger(L, any.a_schar); break;
    }
    return 1;
}

static int
nhl_meta_u_index(L)
lua_State *L;
{
    const char *tkey = luaL_checkstring(L, 2);
    const struct {
        const char *name;
        void *ptr;
        int type;
    } ustruct[] = {
        { "ux", &(u.ux), ANY_UCHAR },
        { "uy", &(u.uy), ANY_UCHAR },
        { "dx", &(u.dx), ANY_SCHAR },
        { "dy", &(u.dy), ANY_SCHAR },
        { "dz", &(u.dz), ANY_SCHAR },
        { "tx", &(u.tx), ANY_UCHAR },
        { "ty", &(u.ty), ANY_UCHAR },
        { "ulevel", &(u.ulevel), ANY_INT },
        { "ulevelmax", &(u.ulevelmax), ANY_INT },
        { "uhunger", &(u.uhunger), ANY_INT },
        { "nv_range", &(u.nv_range), ANY_INT },
        { "xray_range", &(u.xray_range), ANY_INT },
        { "umonster", &(u.umonster), ANY_INT },
        { "umonnum", &(u.umonnum), ANY_INT },
        { "mh", &(u.mh), ANY_INT },
        { "mhmax", &(u.mhmax), ANY_INT },
        { "mtimedone", &(u.mtimedone), ANY_INT },
    };
    int i;

    /* FIXME: doesn't really work, eg. negative values for u.dx */
    for (i = 0; i < SIZE(ustruct); i++)
        if (!strcmp(tkey, ustruct[i].name)) {
            return nhl_push_anything(L, ustruct[i].type, ustruct[i].ptr);
        }

    nhl_error(L, "Unknown u table index");
    return 0;
}

static int
nhl_meta_u_newindex(L)
lua_State *L;
{
    nhl_error(L, "Cannot set u table values");
    return 0;
}

void
init_u_data(L)
lua_State *L;
{
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, nhl_meta_u_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, nhl_meta_u_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    lua_setglobal(L, "u");
}

int
nhl_set_package_path(L, path)
lua_State *L;
const char *path;
{
    lua_getglobal(L, "package");
    lua_pushstring(L, path);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
    return 0;
}

static int
traceback_handler(L)
lua_State *L;
{
    luaL_traceback(L, L, lua_tostring(L, 1), 0);
    /* TODO: call impossible() if fuzzing? */
    return 1;
}

boolean
nhl_loadlua(L, fname)
lua_State *L;
const char *fname;
{
    boolean ret = TRUE;
    dlb *fh;
    char *buf = (char *) 0;
    long buflen;
    int cnt, llret;

    fh = dlb_fopen(fname, "r");
    if (!fh) {
        impossible("nhl_loadlua: Error loading %s", fname);
        ret = FALSE;
        goto give_up;
    }

    dlb_fseek(fh, 0L, SEEK_END);
    buflen = dlb_ftell(fh);
    buf = (char *) alloc(buflen + 1);
    dlb_fseek(fh, 0L, SEEK_SET);

    if ((cnt = dlb_fread(buf, 1, buflen, fh)) != buflen) {
        impossible("nhl_loadlua: Error loading %s, got %i/%li bytes",
                   fname, cnt, buflen);
        ret = FALSE;
        goto give_up;
    }
    buf[buflen] = '\0';
    (void) dlb_fclose(fh);

    llret = luaL_loadstring(L, buf);
    if (llret != LUA_OK) {
        impossible("luaL_loadstring: Error loading %s (errcode %i)",
                   fname, llret);
        ret = FALSE;
        goto give_up;
    } else {
        lua_pushcfunction(L, traceback_handler);
        lua_insert(L, 1);
        if (lua_pcall(L, 0, LUA_MULTRET, -2)) {
            impossible("Lua error: %s", lua_tostring(L, -1));
            ret = FALSE;
            goto give_up;
        }
    }

 give_up:
    if (buf) {
        free(buf);
        buf = (char *) 0;
        buflen = 0;
    }
    return ret;
}

lua_State *
nhl_init()
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    nhl_set_package_path(L, "./?.lua");

    /* register nh -table, and functions for it */
    lua_newtable(L);
    luaL_setfuncs(L, nhl_functions, 0);
    lua_setglobal(L, "nh");

    /* init nhc -table */
    init_nhc_data(L);

    /* init u -table */
    init_u_data(L);

    l_selection_register(L);
    l_register_des(L);

    if (!nhl_loadlua(L, "nhlib.lua")) {
        lua_close(L);
        return (lua_State *) 0;
    }

    return L;
}

boolean
load_lua(name)
const char *name;
{
    boolean ret = TRUE;
    lua_State *L = nhl_init();

    if (!L) {
        ret = FALSE;
        goto give_up;
    }

    if (!nhl_loadlua(L, name)) {
        ret = FALSE;
        goto give_up;
    }

 give_up:
    lua_close(L);

    return ret;
}

const char *
get_lua_version()
{
    size_t len = (size_t) 0;
    const char *vs = (const char *) 0;
    lua_State *L;

    if (g.lua_ver[0] == 0) {
        L = nhl_init();

        if (L) {
            lua_getglobal(L, "_VERSION");
            if (lua_isstring(L, -1))
                vs = lua_tolstring (L, -1, &len);
            if (vs && len < sizeof g.lua_ver) {
                if (!strncmpi(vs, "Lua", 3)) {
                    vs += 3;
                    if (*vs == '-' || *vs == ' ')
                        vs += 1;
                }
                Strcpy(g.lua_ver, vs);
            }
        }
        lua_close(L);
#ifdef LUA_COPYRIGHT
        if (sizeof LUA_COPYRIGHT <= sizeof g.lua_copyright)
            Strcpy(g.lua_copyright, LUA_COPYRIGHT);
#endif
    }
    return (const char *) g.lua_ver;
}
