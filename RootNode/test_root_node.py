import socket
class TestBasicFunctionality:
    def init_branch(self, dut):
        msg_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        ctrl_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        msg_sock.connect(("192.168.2.1", 99))
        msg_sock.send("X3 X1 X0;".encode())
        dut.expect('received NODE_INFO 3 from BranchID=1')
        dut.expect('is msg')
        assert("X0 X0 X1;" in msg_sock.recv(4096).decode())
        ctrl_sock.connect(("192.168.2.1", 99))
        ctrl_sock.send("X2 X1 X0;".encode())
        dut.expect('received NODE_INFO 2 from BranchID=1')
        dut.expect('is ctrl')
        assert("X0 X0 X1;" in ctrl_sock.recv(4096).decode())
        return msg_sock, ctrl_sock
    
    def end_branch(self, dut, msg_sock, ctrl_sock):
        msg_sock.close()
        dut.expect('connection closed on')
        ctrl_sock.close()
        dut.expect('connection closed on')

    def test_timeout(self, dut):
        dut.expect('waiting on poll')
        msg_sock, ctrl_sock = self.init_branch(dut)
        dut.expect('timeout occured')
        dut.expect('sending KEEP_ALIVE to')
        assert('X0 X2 X0;' in ctrl_sock.recv(4096).decode())
        ctrl_sock.send('X3 X0 X1;'.encode())
        msg_sock.send("X3 X3 X0".encode())
        assert("X0 X0 X1;" in msg_sock.recv(4096).decode())
        self.end_branch(dut, msg_sock, ctrl_sock)
        
        
