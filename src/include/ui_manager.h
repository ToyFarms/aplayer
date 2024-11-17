#ifndef __UI_MANAGER_H
#define __UI_MANAGER_H

#include "array.h"
#include "dict.h"
#include "wpl.h"

enum ui_unit_type
{
    OBJ_ATTR_UNIT_INT,
    OBJ_ATTR_UNIT_STR,
};

typedef struct ui_unit
{
    enum ui_unit_type type;
    union {
        struct
        {
            int v;
            bool is_percent;
        } v_int;
        char *v_str;
    };
} ui_unit;

enum ui_element_type
{
    UI_ELEMENT_LAYOUT,
    UI_ELEMENT_WIDGET,
};

typedef struct ui_element
{
    char *name;
    dict_t attr;
    struct ui_element *parent;
    array(struct ui_element) children;
    enum ui_element_type type;
    wpl_instance inst;
} ui_element;

typedef struct ui_scene
{
    ui_element body;
    array(ui_element *) render_order;
} ui_scene;

void ui_render(ui_scene *scene, term_status *term);
void ui_element_print(ui_element *root);
void ui_element_free(ui_element *element);

#endif /* __UI_MANAGER_H */
