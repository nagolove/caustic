typedef struct TrainSet TrainSet;

struct TrainSetSetup {
    int agents_num;
    int inputs, hidden_layers, hidden, outputs;
    void (*model_init)(void *model);
    int (*model_score_get)(void *model);
};

struct TrainSet *tset_new(struct TrainSetSetup *setup);
void tset_shutdown(struct TrainSet *s);
void tset_remove_one(struct TrainSet *s, int index);
int tset_find_min_index(struct TrainSet *s);
struct TrainSet *tset_select(struct TrainSet *s);
void tset_write(struct TrainSet *s);

