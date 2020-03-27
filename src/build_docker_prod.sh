#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

"${DIR}/docker_run.sh" -i ../src/compile_and_cp_bin.sh

PROJECT_NAME=$1

TAG=${2:latest}

# TODO: add -t option with the appropriate image name and tag.
docker build -f Dockerfile.prod -t gcr.io/$PROJECT_NAME/anemomind_anemocppserver:$TAG .

docker build -t gcr.io/$PROJECT_NAME/anemomind_anemowebapp:${TAG} -f www2/Dockerfile \
         --build-arg MONGO_URL=mongodb://anemomongo:27017/anemomind-dev \
         --build-arg GCLOUD_PROJECT=anemomind\
         --build-arg GCS_KEYFILE=/anemomind/www2/anemomind-9b757e3fbacb.json\
         --build-arg GCS_BUCKET=boat_logs\
         --build-arg USE_GS="true" \
         --build-arg PUBSUB_TOPIC_NAME=anemomind_log_topic \
         --build-arg CPP_DOCKER_IMAGE=gcr.io/$PROJECT_NAME/anemomind_anemocppserver:$TAG .

