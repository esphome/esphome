# esphome

![Version: 2024.4.2](https://img.shields.io/badge/Version-2024.4.2-informational?style=flat-square) ![Type: application](https://img.shields.io/badge/Type-application-informational?style=flat-square) ![AppVersion: 2024.4.2](https://img.shields.io/badge/AppVersion-2024.4.2-informational?style=flat-square)

A Helm chart to instal ESPHome in kubernetes

**Homepage:** <https://esphome.io/index.html>

## Maintainers

| Name | Email | Url |
| ---- | ------ | --- |
| XXXX? | <XXXX?> |  |

## Source Code

* <https://github.com/esphome>

## Values

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| affinity | object | `{}` |  |
| esphome | object | `{"settings":{"disable_ha_auth":"","password":"password","relative_url":"","streamer_mode":"","use_mqtt":"","use_ping":"","username":"admin"}}` | configuration for esp_home env variables this values will get into a config map mounted in the container. |
| esphome.settings.password | string | `"password"` | Password for esphome web |
| esphome.settings.use_ping | string | `""` | Use ping to determine devices availability |
| esphome.settings.username | string | `"admin"` | Username for esphome web |
| fullnameOverride | string | `""` |  |
| image.pullPolicy | string | `"IfNotPresent"` |  |
| image.repository | string | `"ghcr.io/esphome/esphome"` |  |
| image.tag | string | `"2024.4.2"` |  |
| imagePullSecrets | list | `[]` |  |
| ingress.annotations | object | `{}` |  |
| ingress.className | string | `""` |  |
| ingress.enabled | bool | `false` |  |
| ingress.hosts[0].host | string | `"chart-example.local"` |  |
| ingress.hosts[0].paths[0].path | string | `"/"` |  |
| ingress.hosts[0].paths[0].pathType | string | `"ImplementationSpecific"` |  |
| ingress.tls | list | `[]` |  |
| nameOverride | string | `""` |  |
| nodeSelector | object | `{}` |  |
| persistence.accessMode | string | `"ReadWriteOnce"` |  |
| persistence.enabled | bool | `false` |  |
| persistence.existingVolume | string | `""` |  |
| persistence.matchExpressions | object | `{}` |  |
| persistence.matchLabels | object | `{}` |  |
| persistence.size | string | `"5Gi"` |  |
| persistence.storageClass | string | `""` |  |
| podAnnotations | object | `{}` |  |
| podLabels | object | `{}` |  |
| podSecurityContext | object | `{}` |  |
| resources | object | `{}` |  |
| securityContext | object | `{}` |  |
| service.port | int | `80` |  |
| service.type | string | `"ClusterIP"` |  |
| serviceAccount.annotations | object | `{}` | Annotations to add to the service account |
| serviceAccount.automount | bool | `true` | Automatically mount a ServiceAccount's API credentials? |
| serviceAccount.create | bool | `true` | Specifies whether a service account should be created |
| serviceAccount.name | string | `""` | The name of the service account to use. If not set and create is true, a name is generated using the fullname template |
| tolerations | list | `[]` |  |
| volumeMounts | list | `[]` |  |
| volumes | list | `[]` |  |

