package main

import (
	"bytes"
	"flag"
	"fmt"
	"math/rand"
	"net"
	"os"
	"sync"
	"time"
)

const (
	payloadChars    = "123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/"
	stringSize      = 9999
	payloadSize     = 1444
	threadCount     = 244
	maxDurationSecs = 480
)

func main() {
	ip := flag.String("ip", "", "Target server IP")
	port := flag.Int("port", 0, "Target server port")
	duration := flag.Int("duration", 30, "Test duration in seconds (max 480)")
	flag.Parse()

	if *ip == "" || *port == 0 {
		fmt.Println("Error: IP and port are required")
		flag.Usage()
		os.Exit(1)
	}

	if *duration > maxDurationSecs {
		fmt.Printf("Error: Duration cannot exceed %d seconds\n", maxDurationSecs)
		os.Exit(1)
	}

	target := fmt.Sprintf("%s:%d", *ip, *port)
	fmt.Printf("Starting UDP load test on %s for %d seconds\n", target, *duration)

	var wg sync.WaitGroup
	stopChan := make(chan struct{})
	rand.Seed(time.Now().UnixNano())

	// Pre-generate payload
	payload := generatePayload()

	// Launch workers
	for i := 0; i < threadCount; i++ {
		wg.Add(1)
		go udpWorker(target, payload, stopChan, &wg)
	}

	// Run for specified duration
	time.Sleep(time.Duration(*duration) * time.Second)
	close(stopChan)
	wg.Wait()

	fmt.Println("Load test completed")
}

func generatePayload() []byte {
	var buf bytes.Buffer
	for i := 0; i < payloadSize; i++ {
		buf.WriteByte(payloadChars[rand.Intn(len(payloadChars))])
	}
	return buf.Bytes()
}

func udpWorker(target string, payload []byte, stopChan <-chan struct{}, wg *sync.WaitGroup) {
	defer wg.Done()

	conn, err := net.Dial("udp", target)
	if err != nil {
		fmt.Printf("Error creating UDP connection: %v\n", err)
		return
	}
	defer conn.Close()

	for {
		select {
		case <-stopChan:
			return
		default:
			_, err := conn.Write(payload)
			if err != nil {
				fmt.Printf("Error sending UDP packet: %v\n", err)
				continue
			}
			// Small delay to prevent complete CPU saturation
			time.Sleep(1 * time.Millisecond)
		}
	}
}