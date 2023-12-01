import 'package:flutter/material.dart';
import 'dart:convert';
import 'package:http/http.dart' as http;


class MyHomePage extends StatefulWidget {
  @override
  _MyHomePageState createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  String imageUrl = 'Your Initial Image URL';

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('ESP32 Image App'),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            Image.network(imageUrl),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: () {
                // Call function to fetch image URL from ESP32
                getImageFromESP32();
              },
              child: Text('Get Image from ESP32'),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> getImageFromESP32() async {
    // Replace with your ESP32 IP address and port
    final response = await http.get('http://your_esp32_ip:your_port/image' as Uri);

    if (response.statusCode == 200) {
      setState(() {
        imageUrl = base64Encode(response.bodyBytes);
      });
    } else {
      throw Exception('Failed to load image');
    }
  }
}
