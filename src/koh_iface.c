#include "koh_iface.h"

#include "koh_dev_draw.h"
/*#include "koh_input.h"*/
#include "koh_logger.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static VContainer *container_head = NULL;
static VContainer *container_active = NULL;

void vcontainer_update_controls(VContainer *c);

static bool empty(void *udata) {
    return false;
}

static Vector2 empty_mouse_position(void *udata) {
    return (Vector2) { 0, 0 };
}

static bool empty_mouse_is_button_pressed(int button, void *udata) {
    return false;
}

static struct IInputSources fill_null_input(struct IInputSources inp) {
    if (!inp.is_down) {
        trace("fill_null_input: is_down setup to empty handler\n");
        inp.is_down = empty;
    }
    if (!inp.is_up) {
        trace("fill_null_input: is_up setup to empty handler\n");
        inp.is_up = empty;
    }
    if (!inp.is_select) {
        trace("fill_null_input: is_select setup to empty handler\n");
        inp.is_select = empty;
    }
    if (!inp.mouse_is_button_pressed) {
        trace("fill_null_input: mouse_is_button_pressed setup to empty handler\n");
        inp.mouse_is_button_pressed = empty_mouse_is_button_pressed;
    }
    if (!inp.mouse_position) {
        trace("fill_null_input: mouse_position setup to empty handler\n");
        inp.mouse_position = empty_mouse_position;
    }
    return inp;
}

VContainer *vcontainer_new(
    int mousebtn_open, const char *name, struct IInputSources inp
) {
    VContainer *cont = calloc(1, sizeof(VContainer));
    assert(cont);

    cont->inp = fill_null_input(inp);
    cont->is_builded = false;
    cont->ctrls = calloc(MAX_CONTROLS, sizeof(Control*));
    cont->color_border = BLACK;
    cont->mousebtn_click = MOUSE_BUTTON_LEFT;
    cont->mousebtn_open = mousebtn_open;
    strncpy(cont->name, name, sizeof(cont->name));
    cont->last_time = GetTime();

    cont->next = container_head;
    container_head = cont;

    return cont;
}

void vcontainer_shutdown(VContainer *c) {
    assert(c);
    VContainer *prev = NULL;
    VContainer *curr = container_head;

    printf("vcontainers_shutdown\n");
    while (curr) {
        printf("name %s\n", curr->name);
        curr = curr->next;
    }
    curr = container_head;

    int j = 0;
    while (curr) {
        printf("vcontainers_shutdown i = %d\n", j++);
        VContainer *next = curr->next;
        // XXX: Тут падает, тестировать
        if (curr == c) {
            if (prev) {
                prev->next = curr->next;
            } else {
                container_head = curr->next;
            }
            break;
        }
        prev = curr;
        curr = next;
    }

    for(int i = 0; i < c->ctrlsnum; i++) {
        if (c->ctrls[i] && c->ctrls[i]->shutdown) {
            c->ctrls[i]->shutdown(c->ctrls[i]);
        }
    }
    free(c->ctrls);
    free(c);
}

Control* vcontainer_add(VContainer *c, Control *control) {
    assert(c);
    assert(control);
    control->container = c;
    c->is_builded = false;

    if (c->ctrlsnum >= MAX_CONTROLS) {
        perror("vcontainer_add: MAX_CONTROLS reached");
        exit(EXIT_FAILURE);
    }

    c->ctrls[c->ctrlsnum++] = control;
    return control;
}

bool vcontainer_update(VContainer *c, void *udata) {
    assert(c);
    assert(c->is_builded);
    Vector2 mousep = c->inp.mouse_position(udata);

    if (!c->is_mnu_open) {
        if (c->mousebtn_open != -1 && 
            c->inp.mouse_is_button_pressed(c->mousebtn_open, udata)) {
            c->mousep = mousep;
            c->pos = mousep;
            c->is_mnu_open = true;
        }
    } else {
        vcontainer_update_controls(c);
        //if (ctx.mousebtn_click != -1 && 
        if (c->inp.mouse_is_button_pressed(c->mousebtn_click, udata)) {
            if (!vcontainer_is_in_area(c, mousep)) {
                c->is_mnu_open = false;
            }
        }

        double now = GetTime();
        double diff = now - c->last_time ;

        //trace("vcontainer_update: diff %f\n", diff);

        if (diff >= 1. / 60.) {
            c->last_time = now;

            if (c->inp.is_up(udata)) {
                c->selected--;
                if (c->selected < 0) 
                    c->selected = c->ctrlsnum - 1;
            } else if (c->inp.is_down(udata)) {
                c->selected++;
                if (c->selected > c->ctrlsnum)
                    c->selected = 0;
            } else if (c->inp.is_select(udata)) {

                Control *ctrl = c->ctrls[c->selected];
                if (ctrl->click)
                    ctrl->click(ctrl);

            }

        }
    }

    return c->is_mnu_open;
}

/*
bool vcontainer_update_gp(VContainer *c, int mousebtn, bool usegamepad) {
    assert(c);
    Vector2 mousep = GetMousePosition();

    if (!c->is_mnu_open) {
        c->is_mnu_open = true;
    } else {
        ContainerContext ctx = { 
            .usegamepad = usegamepad,
            .mousep = c->mousep,
            //.first_iter = first_iter, 
        };
        vcontainer_update_controls(c, &ctx);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (!vcontainer_is_in_area(c, mousep)) {
                c->is_mnu_open = false;
            }
        }
    }

    return c->is_mnu_open;
}
*/

void vcontainer_border_draw(VContainer *c) {
    const int thick = 2;
    Rectangle border = c->border;
    //border.x = ctx->mousep.x;
    //border.y = ctx->mousep.y;
    border.x = c->pos.x;
    border.y = c->pos.y;
    DrawRectangleLinesEx(border, thick, c->color_border);
}

void vcontainer_update_controls(VContainer *c) {
    assert(c);
    /*printf("vcontainer_update_controls: %s\n", c->name);*/

    /*
    if (input.is_up()) {
        if (c->selected >= 1) {
            c->selected--;
        }
    }
    if (input.is_down()) {
        if (c->selected + 1 < c->ctrlsnum) {
            c->selected++;
        }
    }

    if (input.is_select()) {
        Control *ctrl = c->ctrls[c->selected];
        if (ctrl->click) {
            if (ctrl->type == CT_BUTTON) 
                printf("called %s\n", ((Button*)ctrl)->caption);
            ctrl->click(ctrl);
        }
    }
    */

    for(int i = 0; i < c->ctrlsnum; i++) {
        Control *ctrl = c->ctrls[i];
        if (ctrl->update) ctrl->update(ctrl);
    }

    vcontainer_border_draw(c);
}

void btn_set_rect(Control *ctrl, Rectangle rect) {
    if (ctrl) {
        ((Button*)ctrl)->rect = rect;
    }
}

Rectangle btn_get_rect(Control *ctrl) {
    if (ctrl)
        return ((Button*)ctrl)->rect;
    else
        return (Rectangle){ 0, 0, 0, 0};
}

void btn_init(Button *btn) {
    printf("btn_init\n");
}

void btn_shutdown(Button *btn) {
    assert(btn);
    free(btn);
    //printf("btn_done\n");
}

void btn_update(Button *btn) {
    assert(btn);

    //printf("btn_update\n");

    Vector2 point = GetMousePosition();
    Rectangle rect = btn->ctrl.get_rect((Control*)btn);
    Color color = btn->color;

    if (btn->ctrl.container) {
        rect.x += btn->ctrl.container->pos.x;
        rect.y += btn->ctrl.container->pos.y;
    }

    VContainer *container = btn->ctrl.container;

    if (container && container->mousebtn_click != -1) {
        if (CheckCollisionPointRec(point, rect)) {
            //printf("selected %s\n", btn->caption);

            color = btn->color_selected;
            //!ctx->first_iter && 
            if (IsMouseButtonPressed(container->mousebtn_click)) {
                //IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (btn->ctrl.click) {
                    printf("called %s\n", btn->caption);
                    btn->ctrl.click((Control*)btn);
                }
            }
        }
    }

    int index = -1;
    for (int i = 0; i < btn->ctrl.container->ctrlsnum; i++) {
        if (btn->ctrl.container->ctrls[i] == (Control*)btn) {
            index = i;
            break;
        }
    }
    //printf("index = %d\n", index);
    if (btn->ctrl.container->selected == index) {
        //printf("color changed, index %d\n", index);
        color = btn->color_selected;
    }
    // */

    Vector2 origin = {0., 0.};
    DrawRectanglePro(rect, origin, 0., color);
    Vector2 textpos = { .x = rect.x, .y = rect.y };
    DrawTextPro(
        btn->fnt,
        btn->caption,
        textpos,
        Vector2Zero(),
        0., 
        btn->fnt.baseSize,
        0.,
        btn->color_caption
    );
}

Control *btn_new(Button_Def def) {
    Button *btn = calloc(1, sizeof(Button));
    assert(btn);

    if (strlen(def.caption) == 0) {
        printf("btn_new: caption is empty\n");
    }
    strcpy(btn->caption, def.caption);

    btn->ctrl.type = CT_BUTTON;
    btn->ctrl.click = def.click;
    btn->ctrl.init = (ControlFunc)btn_init;
    btn->ctrl.shutdown = (ControlFunc)btn_shutdown;
    btn->ctrl.update = (ControlFunc)btn_update;
    btn->ctrl.get_rect = btn_get_rect;
    btn->ctrl.set_rect = btn_set_rect;
    btn->color = WHITE;
    btn->color_caption = BLACK;
    btn->color_selected = BLUE;
    btn->fnt = def.fnt;

    Vector2 caption = MeasureTextEx(
        btn->fnt,
        btn->caption,
        btn->fnt.baseSize,
        0.
    );

    btn->rect.x = 0;
    btn->rect.y = 0;
    btn->rect.width = caption.x;
    btn->rect.height = caption.y;

    return (Control*)btn;
}

bool vcontainer_is_in_area(VContainer *c, Vector2 pos) {
    assert(c);
    for(int i = 0; i < c->ctrlsnum; i++) {
        Control *ctrl = c->ctrls[i];
        if (!ctrl) {
            continue;
        }
        Rectangle r = {0};
        if (ctrl->get_rect) {
            r = ctrl->get_rect(ctrl);
        }
        if (CheckCollisionPointRec(pos, r)) {
            return true;
        }
    }
    return false;
}

void vcontainer_get_size(VContainer *c, float *width, float *height) {
    assert(c);
    assert(c->is_builded);

    if (width) 
        *width = c->border.width;
    if (height)
        *height = c->border.height;
}

void vcontainer_build(VContainer *c) {
    assert(c);

    int max_width = 0;
    for(int i = 0; i < c->ctrlsnum; ++i) {
        Control *ctrl = c->ctrls[i];
        Rectangle r = ctrl->get_rect(ctrl);
        if (r.width > max_width)
            max_width = r.width;
    }

    Vector2 pos_init = {0, 0};
    for(int i = 0; i < c->ctrlsnum; ++i) {
        Control *ctrl = c->ctrls[i];
        Rectangle r = ctrl->get_rect(ctrl);
        r.x = pos_init.x;
        r.y = pos_init.y;
        r.width = max_width;
        ctrl->set_rect(ctrl, r);
        pos_init.y += r.height;
    }

    c->border.width = max_width;
    c->border.height = pos_init.y;
    c->selected = 0;
    c->is_builded = true;
}

typedef struct xxx_vcontainer_ctx {
    Vector2 mousep, pos;
} xxx_vcontainer_ctx;

static void xxx_draw(void *p) {
    xxx_vcontainer_ctx *ctx = p;
    float radius = 15.;
    DrawCircleV(ctx->pos, radius + 3, VIOLET);
    DrawCircleV(ctx->mousep, radius, BLUE);
}

void vcontainer_open(VContainer *c, Vector2 pos) {
    assert(c);
    printf("vcontainer_open '%s'\n", c->name);
    c->is_mnu_open = true;
    c->mousep = c->pos = pos;

    xxx_vcontainer_ctx ctx = {
        .mousep = pos,
        .pos = pos,
    };

    dev_draw_push(xxx_draw, &ctx, sizeof(ctx));
}

void vcontainers_init() {
    container_head = NULL;
    container_active = NULL;
}

void vcontainers_shutdown() {
    printf("vcontainers_shutdown\n");
}

Button *vcontainer_find(VContainer *c, const char *caption) {
    assert(c);
    assert(caption);
    for (int j = 0; j < c->ctrlsnum; ++j) {
        if (c->ctrls[j]->type == CT_BUTTON) {
            Button *btn = (Button*)c->ctrls[j];
            if (!strcmp(btn->caption, caption)) {
                    return btn;
            }
        }
    }
    return NULL;
}

void vcontainer_next(VContainer *c) {
    printf("vcontainer_next\n");
}

void vcontainer_prev(VContainer *c) {
    printf("vcontainer_prev\n");
}

void vcontainer_ok(VContainer *c) {
    printf("vcontainer_ok\n");
}
