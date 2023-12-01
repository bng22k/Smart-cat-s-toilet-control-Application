import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_storage/firebase_storage.dart';

class MotorControlScreen extends StatefulWidget {
  const MotorControlScreen({Key? key, required this.Title, required String title}) : super(key: key);
  final String Title;

  @override
  _MotorControlScreenState createState() => _MotorControlScreenState();
}

class PhotoData {
  final String photoUrl;
  final DateTime dateTime;

  PhotoData({required this.photoUrl, required this.dateTime});
}

class _MotorControlScreenState extends State<MotorControlScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabController;
  bool isMotorOn = false;
  static const String functionsRegion = 'YOUR_FUNCTIONS_REGION';
  static const String firebaseProjectId = 'YOUR_FIREBASE_PROJECT_ID';

  // Firebase
  late FirebaseAuth _auth;
  late User? _user;
  late FirebaseStorage _storage;

  Future<void> initializeFirebase() async {
    await Firebase.initializeApp();
    _auth = FirebaseAuth.instance;
    _user = _auth.currentUser;
    _storage = FirebaseStorage.instance;
  }

  // Fetch motor status using Firebase Cloud Functions
  Future<void> fetchMotorStatus() async {
    try {
      final response = await http.get(
        Uri.parse(
          'https://$functionsRegion-$firebaseProjectId.cloudfunctions.net/getMotorStatus',
        ),
      );
      if (response.statusCode == 200) {
        setState(() {
          isMotorOn = response.body == 'on';
        });
      }
    } catch (e) {
      print('Error fetching motor status: $e');
    }
  }

  Future<List<PhotoData>> fetchPhotos() async {
    try {
      final storageRef = _storage.ref().child('photos');
      final ListResult result = await storageRef.list();
      final List<Reference> items = result.items;

      List<PhotoData> firebasePhotos = [];

      for (var item in items) {
        final url = await item.getDownloadURL();
        firebasePhotos.add(PhotoData(photoUrl: url, dateTime: DateTime.now()));
      }

      return firebasePhotos;
    } catch (e) {
      print('Error fetching photos from Firebase: $e');
      throw Exception('Failed to fetch photos');
    }
  }

  @override
  void initState() {
    super.initState();
    initializeFirebase();
    _tabController = TabController(vsync: this, length: 2);
    fetchMotorStatus();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Motor Control App'),
        bottom: TabBar(
          controller: _tabController,
          tabs: [
            Tab(text: 'Home'),
            Tab(text: 'Photo'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabController,
        children: [
          // Content for Tab 1
          Center(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: <Widget>[
                Text(
                  'ขณะนี้ได้ตั้งให้ทำงานทุก ${DateTime.now().hour} ชั่วโมง ${DateTime.now().minute} นาที',
                  style: TextStyle(fontSize: 18),
                ),
                ElevatedButton(
                  onPressed: () => print('Toggle Motor'),
                  child: Text(isMotorOn ? 'หยุด' : 'เริ่ม'),
                  style: ElevatedButton.styleFrom(
                    primary: isMotorOn ? Colors.red : Colors.green,
                    onPrimary: Colors.white,
                  ),
                ),
                ElevatedButton(
                  onPressed: () => print('Select Time'),
                  child: Text('ตั้งเวลา'),
                  style: ElevatedButton.styleFrom(
                    primary: Colors.blue,
                    onPrimary: Colors.white,
                  ),
                ),
              ],
            ),
          ),
          // Content for Tab 2
          FutureBuilder<List<PhotoData>>(
            future: fetchPhotos(),
            builder: (context, snapshot) {
              if (snapshot.connectionState == ConnectionState.waiting) {
                return CircularProgressIndicator();
              } else if (snapshot.hasError) {
                return Text('Error: ${snapshot.error}');
              } else if (!snapshot.hasData || snapshot.data!.isEmpty) {
                return Text('No photos available.');
              } else {
                return ListView.builder(
                  itemCount: snapshot.data!.length,
                  itemBuilder: (context, index) {
                    return Card(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Image.network(
                            snapshot.data![index].photoUrl,
                            width: double.infinity,
                            height: 200,
                            fit: BoxFit.cover,
                          ),
                          Padding(
                            padding: const EdgeInsets.all(8.0),
                            child: Text(
                                'Date and Time: ${snapshot.data![index].dateTime.toString()}'),
                          ),
                        ],
                      ),
                    );
                  },
                );
              }
            },
          ),
        ],
      ),
    );
  }
}
