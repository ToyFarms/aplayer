#include "ui_manager.h"
#include "ds.h"

#include <stdlib.h>
#include <string.h>

static void print_obj_attr_unit(ui_unit unit)
{
    if (unit.type == OBJ_ATTR_UNIT_INT)
        printf("%d%s", unit.v_int.v, unit.v_int.is_percent ? "%" : "");
    else
        printf("%s", unit.v_str);
}

static void visualize_tree(ui_element *node, str_t *prefix, int is_last)
{
    if (node == NULL)
        return;

    str_t new_prefix = string_create();
    if (prefix)
    {
        printf("%s%s── ", prefix->buf, is_last ? "└" : "├");
        string_cat_str(&new_prefix, prefix);
        string_cat(&new_prefix, is_last ? "    " : "│   ");
    }

    printf("%s%s(children=%d, attr={",
           node->type == UI_ELEMENT_LAYOUT ? "" : "@", node->name,
           node->children.length);

    for (int i = 0; i < node->attr.bucket_slot; i++)
    {
        array(dict_item) *bkt = &node->attr.buckets[i];
        dict_item item;
        ARR_FOREACH(*bkt, item, j)
        {
            printf("'%s': ", item.key);
            if (strcmp(item.key, "x") == 0 || strcmp(item.key, "y") == 0 ||
                strcmp(item.key, "w") == 0 || strcmp(item.key, "h") == 0 ||
                strcmp(item.key, "anchor-x") == 0 ||
                strcmp(item.key, "anchor-y") == 0)
            {
                ui_unit *unit = dict_get(&node->attr, item.key, NULL);
                if (unit == NULL)
                    continue;

                print_obj_attr_unit(*unit);
                printf(", ");
            }
            else
            {
                ui_unit *unit = dict_get(&node->attr, item.key, NULL);
                printf("[no_repr]%p, ", (void *)unit);
            }
        }
    }
    printf("})\n");

    ui_element *element;
    ARR_FOREACH_BYREF(node->children, element, i)
    {
        visualize_tree(element, &new_prefix, i == node->children.length - 1);
    }

    string_free(&new_prefix);
}

void ui_element_print(ui_element *root)
{
    visualize_tree(root, NULL, 1);
}

void ui_element_free(ui_element *element)
{
    if (element == NULL)
        return;

    dict_free(&element->attr);

    ui_element *child;
    ARR_FOREACH_BYREF(element->children, child, i)
    {
        ui_element_free(child);
    }

    array_free(&element->children);
}

// static void collect_object(ly_object *obj, array(ly_object *) * out)
// {
//     ly_object *child;
//     ARR_FOREACH_BYREF(obj->children, child, i)
//     {
//         array_append(out, child, 1);
//         collect_object(child, out);
//     }
// }
//
// static int ly_object_compare_layer(const void *a, const void *b)
// {
//     if (a == NULL || b == NULL)
//         return 0;
//
//     const ly_object *obj_a = a;
//     const ly_object *obj_b = b;
//
//     return obj_a->layer > obj_b->layer;
// }
//
// static void calculate_render_order(ly_scene *scene)
// {
//     scene->render_order.length = 0;
//     collect_object(&scene->body, &scene->render_order);
//
//     qsort(ARR_AS(scene->render_order, ly_object *),
//     scene->render_order.length,
//           sizeof(ly_object *), ly_object_compare_layer);
// }
//
// static void calculate_object_length(ly_object *obj, int *out)
// {
//     ly_object *child;
//     ARR_FOREACH_BYREF(obj->children, child, i)
//     {
//         if (out)
//             *out += 1;
//         calculate_object_length(child, out);
//     }
// }
//
// void ly_render(ly_scene *scene, term_status *term)
// {
//     int nb_objects = 0;
//     calculate_object_length(&scene->body, &nb_objects);
//     if (nb_objects == 0)
//         return;
//
//     if (scene->render_order.length == 0 ||
//         scene->render_order.length != nb_objects)
//         calculate_render_order(scene);
//
//     ly_object *obj;
//     ARR_FOREACH_BYREF(scene->render_order, obj, i)
//     {
//         wpl_definition def = {
//             .x = obj->pos.x,
//             .y = obj->pos.y,
//             .w = obj->size.x,
//             .h = obj->size.y,
//             .attr = &obj->attr,
//             .theme = NULL,
//         };
//         obj->inst.super->render(obj->inst.ctx, term, &def);
//     }
// }
