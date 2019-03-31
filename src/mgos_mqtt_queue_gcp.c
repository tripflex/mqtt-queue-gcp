#include "mgos_gcp.h"

#include "mgos_mqtt_queue_gcp.h"
#include "common/cs_base64.h"
#include "common/cs_dbg.h"
#include "common/json_utils.h"

#include "frozen.h"
#include "mgos_mongoose_internal.h"
#include "mongoose.h"

#include "mgos_event.h"
#include "mgos_sys_config.h"
#include "mgos_timers.h"

static int s_max_queue = 10;
static int s_current_queue_index = NULL;
static bool s_processing_queue = false;
static mgos_timer_id s_queue_timer_id = MGOS_INVALID_TIMER_ID;
static const char *s_data_path = "";

static void add_ev_handlers(void);
static void remove_ev_handlers(void);

static void clear_queue_timer(void){
    if( s_queue_timer_id != MGOS_INVALID_TIMER_ID){
        mgos_clear_timer(s_queue_timer_id);
    }
    s_queue_timer_id = MGOS_INVALID_TIMER_ID;
}

void static update_queue_index(int next){
    s_current_queue_index = next;
    const char *queue_file_name = "queuemeta.json", *tmp_file_name = "tmpqueuemeta.json";
    char *content = json_fread(queue_file_name);
    FILE *fp = fopen(tmp_file_name, "w");
    struct json_out out = JSON_OUT_FILE(fp);
    json_setf(content, strlen(content), &out, ".i", "%d", next);
    fclose(fp);
    LOG(LL_DEBUG, ("%s %u", "GCP MQTT UPDATE QUEUE NEXT: ", next));
    // json_prettify_file(tmp_file_name); // Optional
    rename(tmp_file_name, queue_file_name);
    if( content != NULL ){
        free(content);
    }
}

static int get_current_queue_index(void){
    // We only need to read from file once, otherwise is stored in memory
    // if( s_current_queue_index != NULL ){
    //     return s_current_queue_index;
    // }

    struct mqtt_queue_meta { int i; } c = { .i = 0 };
    char *content = json_fread("queuemeta.json");
    json_scanf(content, strlen(content), "{i: %d }", &c.i );

    if( content != NULL ){
        free(content);
        s_current_queue_index = c.i;
    } else {
        s_current_queue_index = 0;
    }

    return s_current_queue_index;
}

static int get_next_queue_index(){
    int current = get_current_queue_index();
    int maybe_next = current + 1;
    int next_queue = 1;

    if ( maybe_next <= s_max_queue ){
        next_queue = maybe_next;
    }

    LOG(LL_DEBUG, ("%s %u", "GCP MQTT QUEUE GET NEXT: ", next_queue));
    return next_queue;
}

static void check_queue_timer_cb(void *arg) {
    int index = get_current_queue_index();
    bool res = false;

    if( index < 1 ){
        LOG(LL_DEBUG, ("%s", "GCP MQTT QUEUE - NOTHING QUEUED"));
        remove_ev_handlers();
        clear_queue_timer();
        return;
    }

    char *queue_file;
    char *queue_file_meta;
    mg_asprintf(&queue_file, 0, "%s/queue_%d.json", s_data_path, index);
    mg_asprintf(&queue_file_meta, 0, "%s/queue_%d_meta.json", s_data_path, index);

    char *content = json_fread(queue_file);
    char *content_meta = json_fread(queue_file_meta);

    if (content_meta != NULL && content != NULL) {
        char *subfolder = NULL;
        json_scanf(content_meta, strlen(content_meta), "{subfolder: %Q}", &subfolder );
        res = mgos_gcp_send_event_sub(mg_mk_str(subfolder), mg_mk_str(content));
        free(subfolder);
        free(content_meta);
        free(content);

        remove(queue_file);
        remove(queue_file_meta);
        update_queue_index( index - 1 );
    }

    free(queue_file);
    free(queue_file_meta);
    (void) arg;
}

static void check_queue_ev_cb(int ev, void *ev_data, void *userdata){
    LOG(LL_DEBUG, ("%s", "GCP MQTT CONNECTED - CHECKING QUEUE" ));

    int index = get_current_queue_index();
    if( index < 1 ){
        LOG(LL_DEBUG, ("%s", "GCP MQTT QUEUE - NOTHING QUEUED"));
        remove_ev_handlers();
        clear_queue_timer();
        return;
    }

    // In case connect gets called multiple times
    if( ! s_processing_queue ){
        clear_queue_timer();
        // Use 5 second default timer for now
        s_queue_timer_id = mgos_set_timer(5 * 1000, true, check_queue_timer_cb, NULL);
        s_processing_queue = true;
    }

    (void) ev;
    (void) ev_data;
    (void) userdata;
}

static void stop_queue_ev_cb(int ev, void *ev_data, void *userdata){
    LOG(LL_DEBUG, ("%s", "GCP MQTT DISCONNECTED - STOPPING QUEUE" ));
    clear_queue_timer();
    s_processing_queue = false;
    (void) ev;
    (void) ev_data;
    (void) userdata;
}
static void remove_ev_handlers(void){
    mgos_event_remove_handler(MGOS_GCP_EV_CONNECT, check_queue_ev_cb, NULL);
    mgos_event_remove_handler(MGOS_GCP_EV_CLOSE, stop_queue_ev_cb, NULL);
}

static void add_ev_handlers(void){
    remove_ev_handlers();
    mgos_event_add_handler(MGOS_GCP_EV_CONNECT, check_queue_ev_cb, NULL);
    mgos_event_add_handler(MGOS_GCP_EV_CLOSE, stop_queue_ev_cb, NULL);
}

static int add_to_queue(const char *json_fmt, va_list ap, const char *subfolder){
    int next = get_next_queue_index();
    LOG(LL_DEBUG, ("%s %u", "GCP MQTT QUEUE NOT CONNECTED, QUEUE FILE INDEX: ", next ));

    update_queue_index(next);

    char *new_file = NULL;
    char *new_file_meta = NULL;
    mg_asprintf(&new_file, 0, "%s/queue_%d.json", s_data_path, next);
    mg_asprintf(&new_file_meta, 0, "%s/queue_%d_meta.json", s_data_path, next);

    LOG(LL_DEBUG, ("%s %s", "GCP MQTT QUEUE NOT CONNECTED, ADD TO FILE: ", new_file));
    int result = json_vfprintf((const char*) new_file, json_fmt, ap);
    json_fprintf((const char *) new_file_meta, "{ subfolder: %Q }", subfolder);

    free(new_file);
    free(new_file_meta);

    add_ev_handlers();
    return result;
}

bool mgos_mqtt_queue_gcp_send_event_subf(const char *subfolder, const char *json_fmt, ...) {
  bool res = false;
  va_list ap;
  va_start(ap, json_fmt);

  if ( ! mgos_gcp_is_connected() ){
      LOG(LL_DEBUG, ("%s", "GCP MQTT QUEUE NOT CONNECTED, QUEUE FILE"));
      add_to_queue( json_fmt, ap, subfolder );
  } else {
    char *data = json_vasprintf(json_fmt, ap);

    if (data != NULL) {
        res = mgos_gcp_send_event_sub(mg_mk_str(subfolder), mg_mk_str(data));
        free(data);
    }
  }

  va_end(ap);
  return res;
}

bool mgos_mqtt_queue_gcp_init(void){
    const char *dataPath = mgos_sys_config_get_gcp_queue_data_path();
    s_data_path = (dataPath == NULL) ? "" : dataPath;

    // if( mgos_sys_config_mqttqgcp_enable() ){
    //     s_max_queue = mgos_sys_config_mqttqgcp_max();
        add_ev_handlers();
    // }
     
    return true;
}