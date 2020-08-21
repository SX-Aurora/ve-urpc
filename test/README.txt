How to run the test programs

For normal test
./send_vh 2 I 100 ./recv_ve 1 
./send_vh 2 L 100 ./recv_ve 1 
./send_vh 2 P 100 ./recv_ve 1 
./send_vh 2 Q 100 ./recv_ve 1 
./send_vh 2 Ix 100 ./recv_ve 1 
./send_vh 2 D 100 ./recv_ve 1 
./send_vh 2 ID 100 ./recv_ve 1 

Transfer in maximum buffer (33548264 byte * 2 times)
./send_vh 2 P 33548264 ./recv_ve 1 
./send_vh 2 Q 33548264 ./recv_ve 1 

Transport by reusing buffer (670965 byte * 150 times)
./send_vh 150 P 670965 ./recv_ve 1 
./send_vh 150 Q 670965 ./recv_ve 1 

Variation test of receiving method
./send_vh 2 P 100 ./recv_ve 2
./send_vh 2 P 100 ./recv_ve 3
./send_vh 2 P 100 ./recv_ve 4

For error test
Transfer over maximum buffer (33548265 byte)
./send_vh 2 P 33548265 ./recv_ve 1 
./send_vh 2 Q 33548265 ./recv_ve 1 

Invalid program specification in child process
./send_vh 2 P 100 ./recv_ve2 1

Various errors
./send_vh_e 2 P 100 ./recv_ve 1
./send_vh_e 2 P 100 ./recv_ve 2
./send_vh_e 2 P 100 ./recv_ve 3
./send_vh_e 2 P 100 ./recv_ve 4

For timeout occuerred
./send_vh_t 5 P 33548264 ./recv_ve 1 

