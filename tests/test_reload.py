from wheatserver_test import WheatServer, server_socket, construct_command, PROJECT_PATH
import os
import signal
import time

async_server = None

def test_reload():
    async_server = WheatServer("", "--worker-type %s" % "AsyncWorker",
                               "--app-project-path %s" % os.path.join(PROJECT_PATH, "example"))
    time.sleep(0.5)
    os.kill(async_server.exec_pid, signal.SIGHUP)
    s = server_socket(10829)

    s.send(construct_command("stat", "master"))
    assert "Total workers spawned: 8" in s.recv(1000)
