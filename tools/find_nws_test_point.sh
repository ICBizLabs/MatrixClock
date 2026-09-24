#!/usr/bin/env bash
# Lists NWS alerts currently active somewhere in the US (Severe by default) and prints a lat/lon inside the
# first affected zone, so the clock can be pointed at a live alert for testing. Needs curl and jq.
#   ./tools/find_nws_test_point.sh [severity] [contact-email]
set -euo pipefail
SEV="${1:-Severe}"
UA="matrix-weather-clock-dev (${2:-test@example.com})"
echo "Active ${SEV} alerts:"
curl -s -H "User-Agent: $UA" "https://api.weather.gov/alerts/active?status=actual&severity=${SEV}&limit=10" \
  | jq -r '.features[] | [.properties.event, .properties.areaDesc[0:60], (.properties.affectedZones[0] // "")] | @tsv'
ZONE=$(curl -s -H "User-Agent: $UA" "https://api.weather.gov/alerts/active?status=actual&severity=${SEV}&limit=1" \
  | jq -r '.features[0].properties.affectedZones[0] // empty')
if [ -n "$ZONE" ]; then
  echo
  echo "First zone: $ZONE"
  curl -s -H "User-Agent: $UA" "$ZONE" | jq -r '.geometry.coordinates | flatten | . as $c | "lat=\($c[1]) lon=\($c[0])  (first vertex of the zone polygon)"'
  echo "Point the clock there:  curl -X POST http://matrixclock.local/api/config -H 'Content-Type: application/json' -d '{\"location\":{\"lat\":LAT,\"lon\":LON}}'"
fi
