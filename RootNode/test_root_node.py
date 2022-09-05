import socket
import subprocess
import time
from random import seed
from random import randint

class TestBasicFunctionality:
    
    def send_msg(self, msg_sock, branch_no, content):
        msg = 'X' + str(2 * branch_no + 1) + content
        msg_sock.send(msg.encode())
        
    def send_ctrl(self, ctrl_sock, branch_no, content):
        msg = 'X' + str(2 * branch_no) + content
        ctrl_sock.send(msg.encode())
    def init_branch(self, dut, branch_no):
        err = 0
        while(err < 100):
            output = subprocess.getoutput('sudo /home/predo/predoCode/shell_scripts/root_connect.sh')
            if('successfully activated' in output):
                break
            err += 1
        msg_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        ctrl_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        msg_sock.connect(("192.168.2.1", 99))
        self.send_msg(msg_sock, branch_no, " X1 X0;")
        dut.expect('received NODE_INFO ' + str(branch_no * 2 + 1) + ' from BranchID=' + str(branch_no))
        dut.expect('is msg')
        assert("X0 X0 X1;" in msg_sock.recv(20).decode())
        ctrl_sock.connect(("192.168.2.1", 99))
        self.send_ctrl(ctrl_sock, branch_no, " X1 X0;")
        dut.expect('received NODE_INFO '+ str(branch_no * 2) +' from BranchID=' + str(branch_no))
        dut.expect('is ctrl')
        assert("X0 X0 X1;" in ctrl_sock.recv(20).decode())
        return msg_sock, ctrl_sock
    
    def end_branch(self, dut, msg_sock, ctrl_sock):
        msg_sock.close()
        dut.expect('connection closed on')
        ctrl_sock.close()
        dut.expect('connection closed on')

    def test_timeout(self, dut):
        dut.expect('waiting on poll')
        msg_sock, ctrl_sock = self.init_branch(dut, 1)
        dut.expect('timeout occured')
        dut.expect('sending KEEP_ALIVE to')
        assert('X0 X2 X0;' in ctrl_sock.recv(20).decode())
        ctrl_sock.send('X3 X0 X1;'.encode())
        msg_sock.send("X3 X3 X0".encode())
        assert("X0 X0 X1;" in msg_sock.recv(20).decode())
        self.end_branch(dut, msg_sock, ctrl_sock)

    def test_post(self, dut):
        dut.expect('waiting on poll')
        msg_sock, ctrl_sock = self.init_branch(dut, 1)
        msg_sock.send('X3 X3 X1;'.encode())
        dut.expect('received POST BUTTON 1 from BranchID=1')
        assert('X0 X0 X1;' in msg_sock.recv(20).decode())
        msg_sock.send('X3 X3 X0;'.encode())
        dut.expect('received POST BUTTON 0 from BranchID=1')
        assert('X0 X0 X1;' in msg_sock.recv(20).decode())
        msg_sock.send('X3 X3 X118;'.encode())
        dut.expect('received POST TEMP 24 from BranchID=1')
        assert('X0 X0 X1;' in msg_sock.recv(20).decode())
        msg_sock.send('X3 X3 X119;'.encode())
        dut.expect('received POST TEMP 25 from BranchID=1')
        assert('X0 X0 X1;' in msg_sock.recv(20).decode())
        self.end_branch(dut, msg_sock, ctrl_sock)
    
    def test_inactive(self, dut):
        dut.expect('waiting on poll')
        msg_sock, ctrl_sock = self.init_branch(dut, 1)
        dut.expect('timeout occured')
        dut.expect('sending KEEP_ALIVE to')
        dut.expect('poll timeout occured')
        dut.expect('deleting branch')
        time.sleep(5)
        msg_sock, ctrl_sock = self.init_branch(dut, 1)

    def test_multi_post(self, dut):
        dut.expect('waiting on poll')
        branches = {}
        for i in range(3):
            branches[i] = {'msg': -1, 'ctrl': -1}
            branches[i]['msg'], branches[i]['ctrl'] = self.init_branch(dut, i + 1)
            
        for i in range(3):
            self.send_msg(branches[i]['msg'], i + 1, ' X3 X1;')
            dut.expect('received POST BUTTON 1 from BranchID=' + str(i + 1))
            assert('X0 X0 X1;' in branches[i]['msg'].recv(20).decode())
            self.send_msg(branches[i]['msg'], i + 1, ' X3 X118;')
            dut.expect('received POST TEMP 24 from BranchID=' + str(i + 1))
            assert('X0 X0 X1;' in branches[i]['msg'].recv(20).decode())
        seed(1)
        for i in range(10):
            rand_branch = randint(0, 2)
            rand_button = randint(0, 1)
            rand_temp = randint(0, 9)
            self.send_msg(branches[rand_branch]['msg'], rand_branch + 1, ' X3 X' + str(rand_button) + ';')
            dut.expect('received POST BUTTON '+ str(rand_button) +' from BranchID=' + str(rand_branch + 1))
            assert('X0 X0 X1;' in branches[rand_branch]['msg'].recv(20).decode())
            rand_branch = randint(0, 2)
            self.send_msg(branches[rand_branch]['msg'], rand_branch + 1, ' X3 X10' + str(rand_temp) + ';')
            dut.expect('received POST TEMP '+ str(rand_temp) +' from BranchID=' + str(rand_branch + 1))
            assert('X0 X0 X1;' in branches[rand_branch]['msg'].recv(20).decode())
        for i in range(3):
            self.send_msg(branches[i]['msg'], i + 1, ' X3 X0;')
            dut.expect('received POST BUTTON 0 from BranchID=' + str(i + 1))
            assert('X0 X0 X1;' in branches[i]['msg'].recv(20).decode())
            time.sleep(2)
        for i in range(3):
            self.end_branch(dut, branches[i]['msg'], branches[i]['ctrl'])
        