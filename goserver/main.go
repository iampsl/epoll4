package main

import (
	"encoding/binary"
	"fmt"
	"goserver/ctogo"
	"goserver/tcpsocket"
	"log"
	"net"
	"os"

	"google.golang.org/protobuf/proto"
)

const MSG_HEAD_LEN = 10

func main() {
	log.SetFlags(log.LstdFlags | log.Lshortfile)
	os.Remove("/root/epoll4/release/test.sock")
	ListenAndAccept()
}

func ListenAndAccept() {
	listener, err := net.Listen("unix", "/root/epoll4/release/test.sock")
	if err != nil {
		log.Fatalln(err)
	}
	for {
		tcpConn, err := listener.Accept()
		if err != nil {
			log.Println(err)
			continue
		}
		go NewConnect(tcpConn)
	}
}

func NewConnect(c net.Conn) {
	psocket := tcpsocket.New(c)
	defer psocket.Close()
	readBuffer := make([]byte, 1024*1024*4)
	readSize := 0
	for {
		if readSize == len(readBuffer) {
			log.Printf("msg size > readBuffer(%d)\n", len(readBuffer))
			return
		}
		n, err := psocket.Read(readBuffer[readSize:])
		if err != nil {
			log.Println(err)
			return
		}
		readSize += n
		procTotal := 0
		for {
			proc, err := OnProcess(psocket, readBuffer[procTotal:readSize])
			if err != nil {
				log.Println(err)
				return
			}
			if proc == 0 {
				break
			}
			procTotal += proc
		}
		if procTotal > 0 {
			copy(readBuffer, readBuffer[procTotal:readSize])
			readSize -= procTotal
		}
	}
}

func parseMsgHead(data []byte, plength *uint32, pseq *uint32, pcmd *uint16) {
	*plength = binary.BigEndian.Uint32(data)
	*pseq = binary.BigEndian.Uint32(data[4:])
	*pcmd = binary.BigEndian.Uint16(data[8:])
}

func serialMsgHead(data []byte, length uint32, seq uint32, cmd uint16) {
	binary.BigEndian.PutUint32(data, length)
	binary.BigEndian.PutUint32(data[4:], seq)
	binary.BigEndian.PutUint16(data[8:], cmd)
}

func OnProcess(psocket tcpsocket.Ptr, data []byte) (int, error) {
	if len(data) < MSG_HEAD_LEN {
		return 0, nil
	}
	var length, seq uint32
	var cmd uint16
	parseMsgHead(data, &length, &seq, &cmd)
	if length < MSG_HEAD_LEN {
		return 0, fmt.Errorf("msg length(%d) < %d", length, MSG_HEAD_LEN)
	}
	if int(length) > len(data) {
		return 0, nil
	}
	msgLen := length - MSG_HEAD_LEN
	msgData := make([]byte, msgLen)
	copy(msgData, data[MSG_HEAD_LEN:])
	go OnCmd(psocket, cmd, seq, msgData)
	return int(length), nil
}

func OnCmd(psocket tcpsocket.Ptr, cmd uint16, seq uint32, data []byte) {
	switch cmd {
	case uint16(ctogo.CmdID_QUERY_USER_INFO):
		OnQueryUserInfo(psocket, cmd, seq, data)
	default:
		log.Printf("unknown cmd:%d\n", cmd)
	}
}

func SendMsg(psocket tcpsocket.Ptr, cmd uint16, seq uint32, msg proto.Message) {
	msgBody, err := proto.Marshal(msg)
	if err != nil {
		log.Println(err)
		psocket.Close()
		return
	}
	var msgHead [MSG_HEAD_LEN]byte
	serialMsgHead(msgHead[:], uint32(len(msgBody)+len(msgHead)), seq, cmd)
	psocket.Write(msgHead[:], msgBody)
}

func OnQueryUserInfo(psocket tcpsocket.Ptr, cmd uint16, seq uint32, data []byte) {
	var req ctogo.QueryUserInfoReq
	if err := proto.Unmarshal(data, &req); err != nil {
		log.Println(err)
		psocket.Close()
		return
	}
	var rsp ctogo.QueryUserInfoRsp
	rsp.UserName = "iampsl"
	rsp.Password = "fdfdfd"
	rsp.Money = 100
	SendMsg(psocket, cmd, seq, &rsp)
}
