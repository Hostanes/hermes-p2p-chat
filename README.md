
```
  A peer to peer chat application written in C
```

# hermes-p2p-chat

Rooms are a list of users with 1 user being the Leader

![image](https://github.com/user-attachments/assets/4eb436ce-e049-40ae-9213-5c22f449687b)


Each Client stores a list of Rooms and their users locally.
Each user is stored as a public key and sock-addr information.

if a user wants to join the room they must request to join from the Leader of the room; if accepted, the leader sends over the user list information of the joined room

![image](https://github.com/user-attachments/assets/aeea490e-31e8-4cfa-bb22-0f2d452aa02f)

Packets have a 1 byte Type field at the beginning

Text Messages are encrypted using RSA public/private key encryption

![image](https://github.com/user-attachments/assets/0afd071a-45cd-4d0d-8ee4-229bc4a37a8a)
