// https://gitlab.com/kevinwolfe/esp32_template/-/blob/87ca0ecd615115fdc33013d45b84a237da230575/main/main.c
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include "esp_eth.h"
#include "esp_ota_ops.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

static const char *TAG_ota = "ota_ws";
const char *ota_user;
const char *ota_pass;
char auth_buffer[512];
static bool s_handle_event_disconnected     = false;

typedef struct
{
  const char *username;
  const char *password;
} basic_auth_info_t;

#define HTTPD_401      "401 UNAUTHORIZED"

static basic_auth_info_t auth_info = 
{
  .username = "admin",
  .password = "esp123$",
};

#pragma region html
const char ota_html_file[] = "\
<style>\n\
.progress {margin: 15px auto;  max-width: 500px;height: 30px;}\n\
.progress .progress__bar {\n\
  height: 100%; width: 1%; border-radius: 15px;\n\
  background: repeating-linear-gradient(135deg,#336ffc,#036ffc 15px,#1163cf 15px,#1163cf 30px); }\n\
 .status {font-weight: bold; font-size: 30px;};\n\
</style>\n\
<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/twitter-bootstrap/2.2.1/css/bootstrap.min.css\">\n\
<div class=\"well\" style=\"text-align: center;\">\n\
  <div class=\"btn\" onclick=\"file_sel.click();\"><i class=\"icon-upload\" style=\"padding-right: 5px;\"></i>Upload Firmware</div>\n\
  <div class=\"progress\"><div class=\"progress__bar\" id=\"progress\"></div></div>\n\
  <div class=\"status\" id=\"status_div\"></div>\n\
</div>\n\
<input type=\"file\" id=\"file_sel\" onchange=\"upload_file()\" style=\"display: none;\">\n\
<script>\n\
function upload_file() {\n\
  document.getElementById(\"status_div\").innerHTML = \"Upload in progress\";\n\
  let data = document.getElementById(\"file_sel\").files[0];\n\
  xhr = new XMLHttpRequest();\n\
  xhr.open(\"POST\", \"/ota\", true);\n\
  xhr.setRequestHeader('X-Requested-With', 'XMLHttpRequest');\n\
  xhr.upload.addEventListener(\"progress\", function (event) {\n\
     if (event.lengthComputable) {\n\
    	 document.getElementById(\"progress\").style.width = (event.loaded / event.total) * 100 + \"%\";\n\
     }\n\
  });\n\
  xhr.onreadystatechange = function () {\n\
    if(xhr.readyState === XMLHttpRequest.DONE) {\n\
      var status = xhr.status;\n\
      if (status >= 200 && status < 400)\n\
      {\n\
        document.getElementById(\"status_div\").innerHTML = \"Upload accepted. Device will reboot.\";\n\
      } else {\n\
        document.getElementById(\"status_div\").innerHTML = \"Upload rejected!\";\n\
      }\n\
    }\n\
  };\n\
  xhr.send(data);\n\
  return false;\n\
}\n\
</script>";

const char reset_html_file[] = "\
<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/twitter-bootstrap/2.2.1/css/bootstrap.min.css\">\n\
<div class=\"well\" style=\"text-align: center;\">\n\
  <div class=\"btn\" onclick=\"reset_btn.click();\"><i class=\"icon-wrench\" style=\"padding-right: 5px;\"></i>Reset Device</div>\n\
  <div class=\"status\" id=\"status_div\"></div>\n\
</div>\n\
<input type=\"button\" id=\"reset_btn\" onclick=\"reset_device()\" style=\"display: none;\">\n\
<script>\n\
function reset_device() {\n\
  document.getElementById(\"status_div\").innerHTML = \"Resetting Device...\";\n\
  xhr = new XMLHttpRequest();\n\
  xhr.open(\"POST\", \"/reset\", true);\n\
  xhr.setRequestHeader('X-Requested-With', 'XMLHttpRequest');\n\
  xhr.onreadystatechange = function () {\n\
    if(xhr.readyState === XMLHttpRequest.DONE) {\n\
      var status = xhr.status;\n\
      if (status >= 200 && status < 400)\n\
      {\n\
        document.getElementById(\"status_div\").innerHTML = \"Device is rebooting, reload this page.\";\n\
      } else {\n\
        document.getElementById(\"status_div\").innerHTML = \"Device did NOT reboot?!\";\n\
      }\n\
    }\n\
  };\n\
  xhr.send("");\n\
  return false;\n\
}\n\
</script>";
#pragma endregion

static char *http_auth_basic( const char *username, const char *password )
{
  int out;
  char user_info[128];
  static char digest[512];
  size_t n = 0;
  sprintf( user_info, "%s:%s", username, password );

  esp_crypto_base64_encode( NULL, 0, &n, ( const unsigned char * )user_info, strlen( user_info ) );

  // 6: The length of the "Basic " string
  // n: Number of bytes for a base64 encode format
  // 1: Number of bytes for a reserved which be used to fill zero
  if ( sizeof( digest ) > ( 6 + n + 1 ) )
  {
    strcpy( digest, "Basic " );
    esp_crypto_base64_encode( ( unsigned char * )digest + 6, n, ( size_t * )&out, ( const unsigned char * )user_info, strlen( user_info ) );
  }

  return digest;
}

static esp_err_t ota_get_handler( httpd_req_t *req )
{
  basic_auth_info_t *basic_auth_info = req->user_ctx;

  size_t buf_len = httpd_req_get_hdr_value_len( req, "Authorization" ) + 1;
  if ( ( buf_len > 1 ) && ( buf_len <= sizeof( auth_buffer ) ) )
  {
    if ( httpd_req_get_hdr_value_str( req, "Authorization", auth_buffer, buf_len ) == ESP_OK )
    {
      char *auth_credentials = http_auth_basic( basic_auth_info->username, basic_auth_info->password );
      if ( !strncmp( auth_credentials, auth_buffer, buf_len ) )
      {
        printf( "Authenticated!\n" );
        httpd_resp_set_status( req, HTTPD_200 );
        httpd_resp_set_hdr( req, "Connection", "keep-alive" );
        httpd_resp_send( req, ota_html_file, strlen( ota_html_file ) );
        return ESP_OK;
      }
    }
  }

  printf( "Not authenticated\n" );
  httpd_resp_set_status( req, HTTPD_401 );
  httpd_resp_set_hdr( req, "Connection", "keep-alive" );
  httpd_resp_set_hdr( req, "WWW-Authenticate", "Basic realm=\"Hello\"" );
  httpd_resp_send( req, NULL, 0 );

  return ESP_OK;
}

static esp_err_t ota_post_handler( httpd_req_t *req )
{
  char buf[256];
  httpd_resp_set_status( req, HTTPD_500 );    // Assume failure
  
  int ret, remaining = req->content_len;
  printf( "Receiving\n" );
  
  esp_ota_handle_t update_handle = 0 ;
  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
  // const esp_partition_t *running          = esp_ota_get_running_partition();
  
  if ( update_partition == NULL )
  {
    printf( "Uh oh, bad things\n" );
    goto return_failure;
  }

  // printf( "Writing partition: type %d, subtype %d, offset 0x%08x\n", update_partition-> type, update_partition->subtype, update_partition->address);
  // printf( "Running partition: type %d, subtype %d, offset 0x%08x\n", running->type,           running->subtype,          running->address);
  esp_err_t err = ESP_OK;
  err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
  if (err != ESP_OK)
  {
      printf( "esp_ota_begin failed (%s)", esp_err_to_name(err));
      goto return_failure;
  }
  while ( remaining > 0 )
  {
    // Read the data for the request
    if ( ( ret = httpd_req_recv( req, buf, MIN( remaining, sizeof( buf ) ) ) ) <= 0 )
    {
      if ( ret == HTTPD_SOCK_ERR_TIMEOUT )
      {
        // Retry receiving if timeout occurred
        continue;
      }

      goto return_failure;
    }
    
    size_t bytes_read = ret;
    
    remaining -= bytes_read;
    err = esp_ota_write( update_handle, buf, bytes_read);
    if (err != ESP_OK)
    {
      goto return_failure;
    }
  }

  printf( "Receiving done\n" );

  // End response
  if ( ( esp_ota_end(update_handle)                   == ESP_OK ) && 
       ( esp_ota_set_boot_partition(update_partition) == ESP_OK ) )
  {
    printf( "OTA Success?!\n Rebooting\n" );
    fflush( stdout );

    httpd_resp_set_status( req, HTTPD_200 );
    httpd_resp_send( req, NULL, 0 );
    
    vTaskDelay( 2000 / portTICK_PERIOD_MS);
    esp_restart();
    
    return ESP_OK;
  }
  printf( "OTA End failed (%s)!\n", esp_err_to_name(err));

return_failure:
  if ( update_handle )
  {
    esp_ota_abort(update_handle);
  }

  httpd_resp_set_status( req, HTTPD_500 );    // Assume failure
  httpd_resp_send( req, NULL, 0 );
  return ESP_FAIL;
}

static esp_err_t reset_post_handler( httpd_req_t *req )
{
  printf( "Rebooting\n" );
  fflush( stdout );

  httpd_resp_set_status( req, HTTPD_200 );
  httpd_resp_send( req, NULL, 0 );
  
  vTaskDelay( 2000 / portTICK_PERIOD_MS);
  esp_restart();
  
  return ESP_OK;
}

static esp_err_t reset_get_handler( httpd_req_t *req )
{
  basic_auth_info_t *basic_auth_info = req->user_ctx;

  size_t buf_len = httpd_req_get_hdr_value_len( req, "Authorization" ) + 1;
  if ( ( buf_len > 1 ) && ( buf_len <= sizeof( auth_buffer ) ) )
  {
    if ( httpd_req_get_hdr_value_str( req, "Authorization", auth_buffer, buf_len ) == ESP_OK )
    {
      char *auth_credentials = http_auth_basic( basic_auth_info->username, basic_auth_info->password );
      if ( !strncmp( auth_credentials, auth_buffer, buf_len ) )
      {
        printf( "Authenticated!\n" );
        httpd_resp_set_status( req, HTTPD_200 );
        httpd_resp_set_hdr( req, "Connection", "keep-alive" );
        httpd_resp_send( req, reset_html_file, strlen( reset_html_file ) );
        return ESP_OK;
      }
    }
  }

  printf( "Not authenticated\n" );
  httpd_resp_set_status( req, HTTPD_401 );
  httpd_resp_set_hdr( req, "Connection", "keep-alive" );
  httpd_resp_set_hdr( req, "WWW-Authenticate", "Basic realm=\"Hello\"" );
  httpd_resp_send( req, NULL, 0 );

  return ESP_OK;
}

esp_err_t http_404_error_handler( httpd_req_t *req, httpd_err_code_t err )
{
  httpd_resp_send_err( req, HTTPD_404_NOT_FOUND, "404 error" );
  return ESP_FAIL;  // For any other URI send 404 and close socket
}

static httpd_handle_t start_webserver( void )
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

  // Start the httpd server
  printf( "Starting server on port %d\n", config.server_port );

  if ( httpd_start( &server, &config ) == ESP_OK )
  {
    static const httpd_uri_t ota_post =
    {
      .uri       = "/ota",
      .method    = HTTP_POST,
      .handler   = ota_post_handler,
      .user_ctx  = NULL
    };

    httpd_register_uri_handler( server, &ota_post );
    
    static httpd_uri_t ota_get =
    {
      .uri       = "/ota",
      .method    = HTTP_GET,
      .handler   = ota_get_handler,
      .user_ctx  = &auth_info,
    };
    httpd_register_uri_handler( server, &ota_get );

    static httpd_uri_t reset_post =
    {
      .uri       = "/reset",
      .method    = HTTP_POST,
      .handler   = reset_post_handler,
      .user_ctx  = NULL,
    };
    httpd_register_uri_handler( server, &reset_post );

    static httpd_uri_t reset_get =
    {
      .uri       = "/reset",
      .method    = HTTP_GET,
      .handler   = reset_get_handler,
      .user_ctx  = &auth_info,
    };
    httpd_register_uri_handler( server, &reset_get );
  }
    
  return NULL;
}

static void wifi_task( void *Param )
{
  printf( "Wifi & OTA task starting!\n" );
  
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if ( esp_ota_get_state_partition(running, &ota_state) == ESP_OK )
  {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
      // Validate image some how, then call:
      esp_ota_mark_app_valid_cancel_rollback();
      // If needed: esp_ota_mark_app_invalid_rollback_and_reboot();
    }
  }
  
  httpd_handle_t server = NULL;
  
    printf( "Starting webserver\n" );
    server = start_webserver();

while(1)
  {       
    if ( s_handle_event_disconnected )
    {
      s_handle_event_disconnected = false;

      if ( server )
      {
        printf( "Stopping webserver\n" );
        httpd_stop( server );
        server = NULL;
      }
    }
  }

ESP_LOGI(TAG_ota, "** Debug 1");
}

void init_http( void )
{ 
  // esp_err_t error;
  esp_event_loop_create_default();
  xTaskCreate( wifi_task, "wifi_task", 4096, NULL, 0, NULL );
  // const uint32_t task_delay_ms = 100;
}



