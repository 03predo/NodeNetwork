Repo for the Node Network.

Here is the latest version of the jenkins script for running tests.

    pipeline {
        agent any
        stages {
            stage('Build') {
                steps{
                    sh "sudo /home/predo/predoCode/shell_scripts/wifi_connect.sh  $WIFI_SSID $WIFI_PASS"
                    sh "sudo /home/predo/predoCode/shell_scripts/jenkins_permissions.sh $DEBUG_PORT"
                    sh "git clone https://github.com/03predo/NodeNetwork"
                    sh "git clone https://github.com/03predo/NodeNetworkConfig"
                    sh "cp $ROOT_NODE_PATH/public/inc/node_network.h  $ROOT_NODE_PATH/RootNode/main/inc"
                    sh "cp $ROOT_NODE_PATH/public/src/node_network.c  $ROOT_NODE_PATH/RootNode/main/src"
                    sh "cp /var/lib/jenkins/workspace/RootNode/NodeNetworkConfig/CMakeLists.txt $ROOT_NODE_PATH/RootNode/main"
                    dir('NodeNetwork'){
                        dir('RootNode'){
                            sh "docker run --rm -v $ROOT_NODE_PATH/RootNode:/RootNode -w /RootNode espressif/idf:latest idf.py build"
                        }
                    }
                }
            }
            stage('Flash') {
                steps{
                    dir('NodeNetwork'){
                        dir('RootNode'){
                             sh "docker run --privileged -v /dev:/dev --rm -v $ROOT_NODE_PATH/RootNode:/RootNode -w /RootNode espressif/idf:latest idf.py -p $DEBUG_PORT flash"
                        }
                    }
                }
            }
            stage('Test') {
                steps{
                    dir("NodeNetwork"){
                        dir("RootNode"){
                            withPythonEnv('python3') {
                                sh "pytest --port=$DEBUG_PORT -vv -s"
                            }
                        }
                    }
                }
            }
        }
        post {
            // Clean after build
            always {
                sh "sudo /home/predo/predoCode/shell_scripts/wifi_connect.sh  $WIFI_SSID $WIFI_PASS"
                script{
                    try{
                        dir('NodeNetwork'){
                            dir('RootNode'){
                                sh "docker run --rm -v $ROOT_NODE_PATH/RootNode:/RootNode -w /RootNode espressif/idf:latest idf.py fullclean"
                            }
                        }
                    } catch(error) {
                        sh "echo 'fullclean failed'"
                    }
                }
                cleanWs(cleanWhenNotBuilt: false,
                        deleteDirs: true,
                        disableDeferredWipeout: true,
                        notFailBuild: true)
                dir("${env.WORKSPACE}@tmp") {
                    deleteDir()
                }
            }
        }
    }
confirmation
