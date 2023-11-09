#include "koh_ann_set.h"

#include "genann.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

struct TrainSet {
    genann          **nets;
    //struct ModelBox *models;
    void            **models;
    int             num;
    int (*model_score_get)(void *model);
};

struct TrainSet *tset_new(struct TrainSetSetup *setup) {
    assert(setup);
    struct TrainSet *s = malloc(sizeof(*s));
    s->num = setup->agents_num;
    s->nets = calloc(sizeof(s->nets[0]), setup->agents_num);
    s->models = calloc(sizeof(s->models[0]), setup->agents_num);
    s->model_score_get = setup->model_score_get;
    for (int i = 0; i < s->num; i++) {
        s->nets[i] = genann_init(
            setup->inputs,
            setup->hidden_layers,
            setup->hidden,
            setup->outputs
        );
        setup->model_init(&s->models[i]);
    }
    return s;
}

void set_shutdown(struct TrainSet *s) {
    assert(s);
    for (int i = 0; i < s->num; ++i) {
        genann_free(s->nets[i]);
    }
    free(s->nets);
}

void set_remove_one(struct TrainSet *s, int index) {
    assert(s);
    assert(index >= 0);
    assert(index < s->num);

    genann_free(s->nets[index]);
    s->nets[index] = s->nets[s->num - 1];
    s->models[index] = s->models[s->num - 1];
    --s->num;
}

int set_find_min_index(struct TrainSet *s) {
    assert(sizeof(int) == sizeof(int32_t));
    int min_scores = INT32_MIN;
    int min_index = -1;
    for (int k = 0; k < s->num; k++) {
        //printf("k %d\n", k);
        if (s->model_score_get(s->models[k]) < min_scores) {
            min_scores = s->model_score_get(s->models[k]);
            min_index = k;
        }
    }
    return min_index;
}

struct TrainSet *set_select(struct TrainSet *s) {
    assert(s);
    int num = s->num - 1;
    printf("set_select: num %d\n", num);
    assert(num >= 1);

    const int remove_num = 4;
    for (int i = 0; i < remove_num; i++) {
        int min_index = set_find_min_index(s);
        assert(min_index != -1);
        set_remove_one(s, min_index);
    }

    struct TrainSet *new = malloc(sizeof(*new));
    new->num = s->num;
    new->nets = calloc(sizeof(new->nets[0]), s->num);
    new->models = calloc(sizeof(new->models[0]), s->num);

    for (int i = 0; i < s->num; ++i) {
        new->nets[i] = genann_copy(s->nets[i]);
        new->models[i] = s->models[i];
    }

    return new;
}

void set_train(struct TrainSet *s) {
}

void set_write(struct TrainSet *s) {
    assert(s);
    printf("set_write: num %d\n", s->num);
    for (int j = 0; j < s->num; ++j) {
        char fname[100] = {0};
        sprintf(fname, "bin/trained_%.3d.binary", j);
        FILE *file = fopen(fname, "w");
        if (file) {
            genann_write(s->nets[j], file);
            fclose(file);
        }
    }
}


