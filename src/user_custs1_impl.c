/**
 ****************************************************************************************
 * @file user_custs1_impl.c
 * @brief Peripheral project Custom1 Server implementation source code.
 ****************************************************************************************
 */
#include "gpio.h"
#include "app_api.h"
#include "app.h"
#include "prf_utils.h"
#include "custs1.h"
#include "custs1_task.h"
#include "user_custs1_def.h"
#include "user_custs1_impl.h"
#include "user_multi_peripheral.h"
#include "user_periph_setup.h"
#include "arch_console.h"

#include "adc.h"        // Includes for ADC reading capability
#include "adc_531.h"

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

ke_msg_id_t timer_used                           __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t indication_counter                      __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t non_db_val_read[CFG_MAX_CONNECTIONS]    __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t non_db_val_write[CFG_MAX_CONNECTIONS]   __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

static void timer_cb(void);
static uint16_t gpadc_read(void);
static uint16_t gpadc_sample_to_mv(uint16_t sample);
static timer_hnd timer_id __attribute__((section("retention_mem_area0"),zero_init));

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
static uint16_t gpadc_read(void)
{
    /* Initialize the ADC */
    adc_config_t adc_cfg =
    {
        .input_mode = ADC_INPUT_MODE_SINGLE_ENDED,
        .input = ADC_INPUT_SE_P0_6,
        .smpl_time_mult = 2,
        .continuous = false,
        .interval_mult = 0,
        .input_attenuator = ADC_INPUT_ATTN_4X,
        .chopping = false,
        .oversampling = 0,
    };
    adc_init(&adc_cfg);

    /* Perform offset calibration of the ADC */
    adc_offset_calibrate(ADC_INPUT_MODE_SINGLE_ENDED);

    adc_start();
    uint16_t result = adc_correct_sample(adc_get_sample());
    adc_disable();
    return (result);
}


static uint16_t gpadc_sample_to_mv(uint16_t sample) {
    /* Resolution of ADC sample depends on oversampling rate */
    uint32_t adc_res = 10 + ((6 < adc_get_oversampling()) ? 6 : adc_get_oversampling());

    /* Reference voltage is 900mv but scale based in input attenation */
    uint32_t ref_mv = 900 * (GetBits16(GP_ADC_CTRL2_REG, GP_ADC_ATTN) + 1);

    return (uint16_t)((((uint32_t)sample) * ref_mv) >> adc_res);
}


static void timer_cb(void) {
    /* Perform single ADC conversion */
    uint16_t result = gpadc_read();

    arch_printf("\n\radc result: %dmv", gpadc_sample_to_mv(result));

    /* Restart the timer */
    timer_id = app_easy_timer(200, timer_cb);
}


void user_svc1_ctrl_wr_ind_handler(ke_msg_id_t const msgid,
                                      struct custs1_val_write_ind const *param,
                                      ke_task_id_t const dest_id,
                                      ke_task_id_t const src_id)
{
    uint8_t val = 0;
    memcpy(&val, &param->value[0], param->length);

    if (val != CUSTS1_CP_ADC_VAL1_DISABLE)
    {
        timer_used = app_easy_timer(APP_PERIPHERAL_CTRL_TIMER_DELAY, app_adcval1_timer_cb_handler);
    }
    else
    {
        if (timer_used != EASY_TIMER_INVALID_TIMER)
        {
            app_easy_timer_cancel(timer_used);
            timer_used = EASY_TIMER_INVALID_TIMER;
        }
    }
}

void user_svc1_led_wr_ind_handler(ke_msg_id_t const msgid,
                                     struct custs1_val_write_ind const *param,
                                     ke_task_id_t const dest_id,
                                     ke_task_id_t const src_id)
{
    uint8_t val = 0;
    memcpy(&val, &param->value[0], param->length);

    if (val == CUSTS1_LED_ON)
    {
        GPIO_SetActive(GPIO_LED_PORT, GPIO_LED_PIN);
    }
    else if (val == CUSTS1_LED_OFF)
    {
        GPIO_SetInactive(GPIO_LED_PORT, GPIO_LED_PIN);
    }
}

void user_svc1_long_val_cfg_ind_handler(ke_msg_id_t const msgid,
                                           struct custs1_val_write_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id)
{
    // Generate indication when the central subscribes to it
    if (param->value[0])
    {
        uint8_t conidx = KE_IDX_GET(src_id);

        struct custs1_val_ind_req* req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_IND_REQ,
                                                          prf_get_task_from_id(TASK_ID_CUSTS1),
                                                          TASK_APP,
                                                          custs1_val_ind_req,
                                                          sizeof(indication_counter));

        req->conidx = app_env[conidx].conidx;
        req->handle = SVC1_IDX_INDICATEABLE_VAL;
        req->length = sizeof(indication_counter);
        req->value[0] = (indication_counter >> 8) & 0xFF;
        req->value[1] = indication_counter & 0xFF;

        indication_counter++;

        ke_msg_send(req);
    }
}

void user_svc1_long_val_wr_ind_handler(ke_msg_id_t const msgid,
                                          struct custs1_val_write_ind const *param,
                                          ke_task_id_t const dest_id,
                                          ke_task_id_t const src_id) {}

void user_svc1_long_val_ntf_cfm_handler(ke_msg_id_t const msgid,
                                           struct custs1_val_write_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id) {}

void user_svc1_adc_val_1_cfg_ind_handler(ke_msg_id_t const msgid,
                                            struct custs1_val_write_ind const *param,
                                            ke_task_id_t const dest_id,
                                            ke_task_id_t const src_id) {}

void user_svc1_adc_val_1_ntf_cfm_handler(ke_msg_id_t const msgid,
                                            struct custs1_val_write_ind const *param,
                                            ke_task_id_t const dest_id,
                                            ke_task_id_t const src_id) {}

void user_svc1_button_ntf_cfm_handler(ke_msg_id_t const msgid,
                                         struct custs1_val_write_ind const *param,
                                         ke_task_id_t const dest_id,
                                         ke_task_id_t const src_id) {}

void user_svc1_indicateable_cfg_ind_handler(ke_msg_id_t const msgid,
                                               struct custs1_val_write_ind const *param,
                                               ke_task_id_t const dest_id,
                                               ke_task_id_t const src_id) {}

void user_svc1_indicateable_ind_cfm_handler(ke_msg_id_t const msgid,
                                               struct custs1_val_write_ind const *param,
                                               ke_task_id_t const dest_id,
                                               ke_task_id_t const src_id) {}

void user_svc1_long_val_att_info_req_handler(ke_msg_id_t const msgid,
                                                struct custs1_att_info_req const *param,
                                                ke_task_id_t const dest_id,
                                                ke_task_id_t const src_id) {
    struct custs1_att_info_rsp *rsp = KE_MSG_ALLOC(CUSTS1_ATT_INFO_RSP,
                                                   src_id,
                                                   dest_id,
                                                   custs1_att_info_rsp);
    rsp->conidx  = app_env[param->conidx].conidx;
    rsp->att_idx = param->att_idx;
    rsp->length  = 0;
    rsp->status  = ATT_ERR_NO_ERROR;

    ke_msg_send(rsp);
}

void user_svc1_rest_att_info_req_handler(ke_msg_id_t const msgid,
                                            struct custs1_att_info_req const *param,
                                            ke_task_id_t const dest_id,
                                            ke_task_id_t const src_id) {
    struct custs1_att_info_rsp *rsp = KE_MSG_ALLOC(CUSTS1_ATT_INFO_RSP,
                                                   src_id,
                                                   dest_id,
                                                   custs1_att_info_rsp);
    rsp->conidx  = app_env[param->conidx].conidx;
    rsp->att_idx = param->att_idx;
    rsp->length  = 0;
    rsp->status  = ATT_ERR_WRITE_NOT_PERMITTED;
    ke_msg_send(rsp);
}

void app_adcval1_timer_cb_handler() {
    struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
                                                          prf_get_task_from_id(TASK_ID_CUSTS1),
                                                          TASK_APP,
                                                          custs1_val_ntf_ind_req,
                                                          DEF_SVC1_ADC_VAL_1_CHAR_LEN);

    // ADC value to be sampled
    // static uint16_t sample      __SECTION_ZERO("retention_mem_area0");
    // sample = (sample <= 0xffff) ? (sample + 1) : 0;

    // //req->conhdl = app_env->conhdl;
    // req->handle = SVC1_IDX_ADC_VAL_1_VAL;
    // req->length = DEF_SVC1_ADC_VAL_1_CHAR_LEN;
    // req->notification = true;
    // memcpy(req->value, &sample, DEF_SVC1_ADC_VAL_1_CHAR_LEN);

    // ke_msg_send(req);

    // if (ke_state_get(TASK_APP) == APP_CONNECTED)
    // {
    //     // Set it once again until Stop command is received in Control Characteristic
    //     timer_used = app_easy_timer(APP_PERIPHERAL_CTRL_TIMER_DELAY, app_adcval1_timer_cb_handler);
    // }

    uint16_t out[100];
    for (int i = 0; i < 100; i++) {
        uint16_t output = gpadc_sample_to_mv(gpadc_read()); // Get uint16_t ADC reading
        out[i] = output;
    }

    req->handle = SVC1_IDX_ADC_VAL_1_VAL;      // Location to send it to
    req->length = DEF_SVC1_ADC_VAL_1_CHAR_LEN;
    req->notification = true;
    memcpy(req->value, &out, DEF_SVC1_ADC_VAL_1_CHAR_LEN);
    ke_msg_send(req);

    if (ke_state_get(TASK_APP) == APP_CONNECTED) {timer_used = app_easy_timer(10, app_adcval1_timer_cb_handler);};
}

void user_svc2_write_1_val_wr_ind_handler(ke_msg_id_t const msgid,
                                       struct custs1_val_write_ind const *param,
                                       ke_task_id_t const dest_id,
                                       ke_task_id_t const src_id) {
    memcpy(&non_db_val_write[sdkconidx_to_appconidx(param->conidx)], param->value, param->length);
    printout_db_status();
}

void user_svc3_read_non_db_val_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id)
{ 
    non_db_val_read[sdkconidx_to_appconidx(param->conidx)]++;
    printout_db_status();
    struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC_DYN(CUSTS1_VALUE_REQ_RSP,
                                                        prf_get_task_from_id(TASK_ID_CUSTS1),
                                                        TASK_APP,
                                                        custs1_value_req_rsp,
                                                        DEF_SVC3_READ_VAL_1_CHAR_LEN);
    rsp->conidx  = param->conidx;
    rsp->att_idx = param->att_idx;
    rsp->length  = sizeof(uint16_t);
    rsp->status  = ATT_ERR_NO_ERROR;
    memcpy(&rsp->value, &non_db_val_read[sdkconidx_to_appconidx(param->conidx)], rsp->length);
    ke_msg_send(rsp);
}

void user_svc2_write_non_db_val_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id) {
    struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC_DYN(CUSTS1_VALUE_REQ_RSP,
                                                        prf_get_task_from_id(TASK_ID_CUSTS1),
                                                        TASK_APP,
                                                        custs1_value_req_rsp,
                                                        DEF_SVC2_WRITE_VAL_1_CHAR_LEN);

    // Provide the connection index.
    rsp->conidx  = param->conidx;
    // Provide the attribute index.
    rsp->att_idx = param->att_idx;
    // Force current length to zero.
    rsp->length  = sizeof(uint16_t);
    // Provide the ATT error code.
    rsp->status  = ATT_ERR_NO_ERROR;
    // Copy value
    memcpy(&rsp->value, &non_db_val_write[sdkconidx_to_appconidx(param->conidx)], rsp->length);
    // Send message
    ke_msg_send(rsp);
}
