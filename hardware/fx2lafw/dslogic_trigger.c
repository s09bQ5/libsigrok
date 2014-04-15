/*
 * This file is part of the DSLogic project.
 */

#include "protocol.h"
#include <assert.h>

/**
 * @file
 *
 * init, set DSLogic trigger.
 */

/**
 * @defgroup Trigger handling
 *
 * init, set DSLogic trigger
 *
 * @{
 */

struct ds_trigger *trigger;

/**
 * recovery trigger to initial status.
 *
 * @return SR_OK upon success.
 */
SR_PRIV int ds_trigger_init(struct ds_trigger *trigger)
{
    int i, j;

    trigger->trigger_en = 0;
    trigger->trigger_mode = DSLOGIC_TRIGGER_SIMPLE;
    trigger->trigger_pos = 0;
    trigger->trigger_stages = 0;

    for (i = 0; i <= DSLOGIC_TRIGGER_STAGES; i++) {
        for (j = 0; j < DSLOGIC_TRIGGER_PROBES; j++) {
            trigger->trigger0[i][j] = 'X';
            trigger->trigger1[i][j] = 'X';
        }
        trigger->trigger0_count[i] = 0;
        trigger->trigger1_count[i] = 0;
        trigger->trigger0_inv[i] = 0;
        trigger->trigger1_inv[i] = 0;
        trigger->trigger_logic[i] = 1;
    }


    return SR_OK;
}

/**
 * set trigger based on stage
 *
 * @return SR_OK upon success.
 */
SR_PRIV int ds_trigger_stage_set_value(struct ds_trigger *trigger, uint16_t stage, uint16_t probes, char *trigger0, char *trigger1)
{
    assert(stage < DSLOGIC_TRIGGER_STAGES);
    assert(probes <= DSLOGIC_TRIGGER_PROBES);

    int j;

    for (j = 0; j< probes; j++) {
        trigger->trigger0[stage][probes - j - 1] = *(trigger0 + j * 2);
        trigger->trigger1[stage][probes - j - 1] = *(trigger1 + j * 2);
    }

    return SR_OK;
}

SR_PRIV int ds_trigger_stage_set_logic(struct ds_trigger *trigger, uint16_t stage, uint16_t probes, unsigned char trigger_logic)
{
    assert(stage < DSLOGIC_TRIGGER_STAGES);
    assert(probes <= DSLOGIC_TRIGGER_PROBES);

    trigger->trigger_logic[stage] = trigger_logic;

    return SR_OK;
}

SR_PRIV int ds_trigger_stage_set_inv(struct ds_trigger *trigger, uint16_t stage, uint16_t probes, unsigned char trigger0_inv, unsigned char trigger1_inv)
{
    assert(stage < DSLOGIC_TRIGGER_STAGES);
    assert(probes <= DSLOGIC_TRIGGER_PROBES);

    trigger->trigger0_inv[stage] = trigger0_inv;
    trigger->trigger1_inv[stage] = trigger1_inv;

    return SR_OK;
}

SR_PRIV int ds_trigger_stage_set_count(struct ds_trigger *trigger, uint16_t stage, uint16_t probes, uint16_t trigger0_count, uint16_t trigger1_count)
{
    assert(stage < DSLOGIC_TRIGGER_STAGES);
    assert(probes <= DSLOGIC_TRIGGER_PROBES);

    trigger->trigger0_count[stage] = trigger0_count;
    trigger->trigger1_count[stage] = trigger1_count;

    return SR_OK;
}

/**
 * set trigger based on probe
 *
 * @return SR_OK upon success.
 */
SR_PRIV int ds_trigger_probe_set(struct ds_trigger *trigger, uint16_t probe, unsigned char trigger0, unsigned char trigger1)
{
    assert(probe < DSLOGIC_TRIGGER_PROBES);

    trigger->trigger0[DSLOGIC_TRIGGER_STAGES][probe] = trigger0;
    trigger->trigger1[DSLOGIC_TRIGGER_STAGES][probe] = trigger1;

    return SR_OK;
}

/**
 * set trigger stage
 *
 * @return SR_OK upon success.
 */
SR_PRIV int ds_trigger_set_stage(struct ds_trigger *trigger, uint16_t stages)
{
    assert(stages <= DSLOGIC_TRIGGER_STAGES);

    trigger->trigger_stages = stages;

    return SR_OK;
}

/**
 * set trigger position
 *
 * @return SR_OK upon success.
 */
SR_PRIV int ds_trigger_set_pos(struct ds_trigger *trigger, uint16_t position)
{
    assert(position <= 100);

    trigger->trigger_pos = position;

    return SR_OK;
}

/**
 * set trigger en
 *
 * @return SR_OK upon success.
 */
SR_PRIV int ds_trigger_set_en(struct ds_trigger *trigger, uint16_t enable)
{

    trigger->trigger_en = enable;

    return SR_OK;
}

/**
 * set trigger mode
 *
 * @return SR_OK upon success.
 */
SR_PRIV int ds_trigger_set_mode(struct ds_trigger *trigger, uint16_t mode)
{

    trigger->trigger_mode = mode;

    return SR_OK;
}

/*
 *
 */
SR_PRIV uint64_t ds_trigger_get_mask0(struct ds_trigger *trigger, uint16_t stage)
{
    assert(stage <= DSLOGIC_TRIGGER_STAGES);

    uint64_t mask = 0;
    int i;

    for (i = DSLOGIC_TRIGGER_PROBES - 1; i >= 0 ; i--) {
        mask = (mask << 1);
        mask += ((trigger->trigger0[stage][i] == 'X') |
                (trigger->trigger0[stage][i] == 'C'));
    }

    return mask;
}
SR_PRIV uint64_t ds_trigger_get_mask1(struct ds_trigger *trigger, uint16_t stage)
{
    assert(stage <= DSLOGIC_TRIGGER_STAGES);

    uint64_t mask = 0;
    int i;

    for (i = DSLOGIC_TRIGGER_PROBES - 1; i >= 0 ; i--) {
        mask = (mask << 1);
        mask += ((trigger->trigger1[stage][i] == 'X') |
                (trigger->trigger1[stage][i] == 'C'));
    }

    return mask;
}

SR_PRIV uint64_t ds_trigger_get_value0(struct ds_trigger *trigger, uint16_t stage)
{
    assert(stage <= DSLOGIC_TRIGGER_STAGES);

    uint64_t value = 0;
    int i;

    for (i = DSLOGIC_TRIGGER_PROBES - 1; i >= 0 ; i--) {
        value = (value << 1);
        value += ((trigger->trigger0[stage][i] == '1') |
                (trigger->trigger0[stage][i] == 'R'));
    }

    return value;
}

SR_PRIV uint64_t ds_trigger_get_value1(struct ds_trigger *trigger, uint16_t stage)
{
    assert(stage <= DSLOGIC_TRIGGER_STAGES);

    uint64_t value = 0;
    int i;

    for (i = DSLOGIC_TRIGGER_PROBES - 1; i >= 0 ; i--) {
        value = (value << 1);
        value += ((trigger->trigger1[stage][i] == '1') |
                (trigger->trigger1[stage][i] == 'R'));
    }

    return value;
}

SR_PRIV uint64_t ds_trigger_get_edge0(struct ds_trigger *trigger, uint16_t stage)
{
    assert(stage <= DSLOGIC_TRIGGER_STAGES);

    uint64_t edge = 0;
    int i;

    for (i = DSLOGIC_TRIGGER_PROBES - 1; i >= 0 ; i--) {
        edge = (edge << 1);
        edge += ((trigger->trigger0[stage][i] == 'R') |
                (trigger->trigger0[stage][i] == 'F') |
                (trigger->trigger0[stage][i] == 'C'));
    }

    return edge;
}

SR_PRIV uint64_t ds_trigger_get_edge1(struct ds_trigger *trigger, uint16_t stage)
{
    assert(stage <= DSLOGIC_TRIGGER_STAGES);

    uint64_t edge = 0;
    int i;

    for (i = DSLOGIC_TRIGGER_PROBES - 1; i >= 0 ; i--) {
        edge = (edge << 1);
        edge += ((trigger->trigger1[stage][i] == 'R') |
                (trigger->trigger1[stage][i] == 'F') |
                (trigger->trigger1[stage][i] == 'C'));
    }

    return edge;
}

/** @} */
