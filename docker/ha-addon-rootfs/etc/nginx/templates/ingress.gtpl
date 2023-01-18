server {
    listen {{ .interface }}:{{ .port }} default_server;

    include /etc/nginx/includes/server_params.conf;
    include /etc/nginx/includes/proxy_params.conf;

    # Set Home Assistant Ingress header
    proxy_set_header X-HA-Ingress "YES";

    location / {
        allow   172.30.32.2;
        deny    all;

        proxy_pass http://esphome;
    }
}
