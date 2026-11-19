/* Legacy URLs remain compatible; no captive portal or DNS interception. */
#include "captive_portal.h"
#include <string.h>

static esp_err_t controller_redirect(httpd_req_t *req)
{
    httpd_resp_set_status(req,"302 Found");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"Location","/");
    return httpd_resp_sendstr(req,"Open the controller.");
}
static esp_err_t retired_portal_api(httpd_req_t *req)
{
    httpd_resp_set_status(req,"410 Gone");
    httpd_resp_set_type(req,"application/json");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"portal_removed\",\"controlUrl\":\"/\"}");
}
static esp_err_t captive_status(httpd_req_t *req)
{
    httpd_resp_set_type(req,"application/captive+json");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,"{\"captive\":false}");
}
esp_err_t captive_portal_start(httpd_handle_t server)
{
    /* Do not install a DNS task, DHCP portal URL, or wildcard redirect.
     * A visitor opens the controller directly using its displayed address. */
    const httpd_uri_t routes[]={
        {.uri="/portal",.method=HTTP_GET,.handler=controller_redirect},
        {.uri="/portal/",.method=HTTP_GET,.handler=controller_redirect},
        {.uri="/api/portal",.method=HTTP_GET,.handler=retired_portal_api},
        {.uri="/api/portal",.method=HTTP_POST,.handler=retired_portal_api},
        {.uri="/.well-known/captive-portal",.method=HTTP_GET,.handler=captive_status},
    };
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);++i){
        esp_err_t err=httpd_register_uri_handler(server,&routes[i]);
        if(err!=ESP_OK)return err;
    }
    return ESP_OK;
}
