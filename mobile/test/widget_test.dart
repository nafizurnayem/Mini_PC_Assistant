import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pc_bot/app_theme.dart';

void main() {
  testWidgets('gaming theme and responsive backdrop render', (tester) async {
    await tester.pumpWidget(MaterialApp(
      theme: buildBotTheme(),
      home: const GridBackdrop(
        child: Scaffold(body: Center(child: Text('PC // BOT'))),
      ),
    ));

    expect(find.text('PC // BOT'), findsOneWidget);
    expect(Theme.of(tester.element(find.text('PC // BOT'))).brightness,
        Brightness.dark);
  });
}
