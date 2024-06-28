#include "koh_b2_world.h"

#include "koh_common.h"

extern bool b2_parallel;

void world_step(struct WorldCtx *wctx) {
    if (!wctx->is_paused)
        b2World_Step(wctx->world, wctx->timestep, wctx->substeps);
}

static void* enqueue_task(
    b2TaskCallback* task, int32_t itemCount, int32_t minRange,
    void* taskContext, void* userContext
) {
    /*
    struct Stage_Box2d *st = userContext;
    if (st->tasks_count < tasks_max)
    {
        SampleTask& sampleTask = sample->m_tasks[sample->m_taskCount];
        sampleTask.m_SetSize = itemCount;
        sampleTask.m_MinRange = minRange;
        sampleTask.m_task = task;
        sampleTask.m_taskContext = taskContext;
        sample->m_scheduler.AddTaskSetToPipe(&sampleTask);
        ++sample->m_taskCount;
        return &sampleTask;
    }
    else
    {
        assert(false);
        task(0, itemCount, 0, taskContext);
        return NULL;
    }
    */
    return NULL;
}

static void finish_task(void* taskPtr, void* userContext) {
    /*
    SampleTask* sampleTask = static_cast<SampleTask*>(taskPtr);
    Sample* sample = static_cast<Sample*>(userContext);
    sample->m_scheduler.WaitforTask(sampleTask);
    */
}

void world_init(struct WorldCtxSetup *setup, struct WorldCtx *wctx) {
    assert(setup);
    assert(wctx);
    /*b2_parallel = false;*/

    if (setup->wd) 
        wctx->world_def = *setup->wd;
    else {
        wctx->world_def =  b2DefaultWorldDef();
        wctx->world_def.enableContinous = true;
        wctx->world_def.enableSleep = true;
        wctx->world_def.gravity = b2Vec2_zero;
    }

    wctx->timestep = 1. / 60.;
    //int max_threads = 6;
    wctx->task_shed = enkiNewTaskScheduler();
    enkiInitTaskScheduler(wctx->task_shed);
    assert(wctx->task_shed);
    wctx->task_set = enkiCreateTaskSet(wctx->task_shed, NULL);
    assert(wctx->task_set);
    wctx->world_def.enqueueTask = enqueue_task;
    wctx->world_def.finishTask = finish_task;

    wctx->world = b2CreateWorld(&wctx->world_def);

    wctx->width = setup->width;
    wctx->height = setup->height;

    assert(setup->xrng);
    wctx->xrng = setup->xrng;
    wctx->substeps = 4;

    wctx->world_dbg_draw = b2_world_dbg_draw_create();
    wctx->is_dbg_draw = false;

    trace(
        "world_init: width %u, height %u, "
        "xorshift32_state seed %u,"
        "gravity %s\n",
        setup->width, setup->height, 
        setup->xrng->a,
        b2Vec2_to_str(b2World_GetGravity(wctx->world))
    );
}

void world_shutdown(struct WorldCtx *wctx) {
    trace("world_shutdown: wctx %p\n", wctx);
    if (!wctx) {
        return;
    }

    if (wctx->task_set) {
        enkiDeleteTaskSet(wctx->task_shed, wctx->task_set);
        wctx->task_set = NULL;
    }

    if (wctx->task_shed) {
        enkiDeleteTaskScheduler(wctx->task_shed);
        wctx->task_shed = NULL;
    }

    b2WorldId zero_world = {};
    if (!memcmp(&wctx->world, &zero_world, sizeof(zero_world))) {
        b2DestroyWorld(wctx->world);
        memset(&wctx->world, 0, sizeof(wctx->world));
    }

    // FIXME: Нужно-ли специально удалять физические тела?
    // Возможно добавить проход по компонентам тел в системе.

    memset(wctx, 0, sizeof(*wctx));
}


