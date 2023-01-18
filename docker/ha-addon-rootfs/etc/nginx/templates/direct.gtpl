server {
    {{ if not .ssl }}
    listen 6052 default_server;
    {{ else }}
    listen 6052 default_server ssl http2;
    {{ end }}

    include /etc/nginx/includes/server_params.conf;
    include /etc/nginx/includes/proxy_params.conf;

    {{ if .ssl }}
    include /etc/nginx/includes/ssl_params.conf;

    ssl_certificate /ssl/{{ .certfile }};
    ssl_certificate_key /ssl/{{ .keyfile }};

    # Redirect http requests to https on the same port.
    # https://rageagainstshell.com/2016/11/redirect-http-to-https-on-the-same-port-in-nginx/
    error_page 497 https://$http_host$request_uri;
    {{ end }}

    # Clear Home Assistant Ingress header
    proxy_set_header X-HA-Ingress "";

    location / {
        proxy_pass http://esphome;
    }
}
