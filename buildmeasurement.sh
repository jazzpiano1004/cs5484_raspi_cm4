set -e
VERSION=$1
PREFIX=intelligentscada.com
PREFIX_IMAGE=smartmeter
sudo VERSION=$1 su -c 'printf "%s" $VERSION > measurementversion.txt'
echo Create Image Version $VERSION
sudo docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
sudo docker build --pull --platform amd64 -f Dockerfile_measurement_amd64 . -t $PREFIX:5000/$PREFIX_IMAGE/measurement-amd64:$VERSION
sudo docker build --pull --platform aarch64 -f Dockerfile_measurement_arm64 . -t $PREFIX:5000/$PREFIX_IMAGE/measurement-arm64:$VERSION
sudo docker build --pull --platform armhf -f Dockerfile_measurement_armhf . -t $PREFIX:5000/$PREFIX_IMAGE/measurement-armhf:$VERSION
sudo docker run --rm --privileged multiarch/qemu-user-static --reset 
sudo docker push $PREFIX:5000/$PREFIX_IMAGE/measurement-amd64:$VERSION
sudo docker push $PREFIX:5000/$PREFIX_IMAGE/measurement-arm64:$VERSION
sudo docker push $PREFIX:5000/$PREFIX_IMAGE/measurement-armhf:$VERSION
sudo docker manifest create \
    $PREFIX:5000/$PREFIX_IMAGE/measurement:$VERSION \
    $PREFIX:5000/$PREFIX_IMAGE/measurement-arm64:$VERSION \
    $PREFIX:5000/$PREFIX_IMAGE/measurement-armhf:$VERSION \
    $PREFIX:5000/$PREFIX_IMAGE/measurement-amd64:$VERSION 

sudo docker manifest push --purge $PREFIX:5000/$PREFIX_IMAGE/measurement:$VERSION
sudo docker manifest create \
    $PREFIX:5000/$PREFIX_IMAGE/measurement:latest \
    $PREFIX:5000/$PREFIX_IMAGE/measurement-arm64:$VERSION \
    $PREFIX:5000/$PREFIX_IMAGE/measurement-armhf:$VERSION \
    $PREFIX:5000/$PREFIX_IMAGE/measurement-amd64:$VERSION 
sudo docker manifest push --purge $PREFIX:5000/$PREFIX_IMAGE/measurement:latest
