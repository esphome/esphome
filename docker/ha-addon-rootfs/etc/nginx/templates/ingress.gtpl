server {
    listen 127.0.0.1:{{ .port }} default_server;
    listen {{ .interface }}:{{ .port }} default_server;

    include /etc/nginx/includes/server_params.conf;
    include /etc/nginx/includes/proxy_params.conf;

    # Set Home Assistant Ingress header
    proxy_set_header X-HA-Ingress "YES";

    location / {
        allow   172.30.32.2;
        allow   127.0.0.1;
        deny    all;

        proxy_pass http://esphome;
    }
}
