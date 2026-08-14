import 'package:flutter/material.dart';

abstract final class BotColors {
  static const background = Color(0xff050914);
  static const surface = Color(0xff0b1424);
  static const surfaceHigh = Color(0xff11213a);
  static const edge = Color(0xff243b5d);
  static const cyan = Color(0xff25e6ff);
  static const green = Color(0xff4dff9a);
  static const pink = Color(0xffff3cac);
  static const warning = Color(0xffffb84d);
}

ThemeData buildBotTheme() {
  final scheme = ColorScheme.fromSeed(
    seedColor: BotColors.cyan,
    brightness: Brightness.dark,
    surface: BotColors.surface,
    error: const Color(0xffff5d7d),
  );
  return ThemeData(
    colorScheme: scheme,
    scaffoldBackgroundColor: BotColors.background,
    useMaterial3: true,
    fontFamily: 'Roboto',
    appBarTheme: const AppBarTheme(
      backgroundColor: BotColors.background,
      foregroundColor: Colors.white,
      centerTitle: false,
      elevation: 0,
    ),
    cardTheme: CardThemeData(
      color: BotColors.surface,
      elevation: 0,
      margin: EdgeInsets.zero,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(18),
        side: const BorderSide(color: BotColors.edge),
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: BotColors.surface,
      border: OutlineInputBorder(borderRadius: BorderRadius.circular(12)),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: BotColors.edge),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: BotColors.cyan, width: 1.5),
      ),
    ),
    navigationBarTheme: const NavigationBarThemeData(
      backgroundColor: BotColors.surface,
      indicatorColor: Color(0x3325e6ff),
    ),
  );
}

class GridBackdrop extends StatelessWidget {
  const GridBackdrop({required this.child, super.key});
  final Widget child;

  @override
  Widget build(BuildContext context) => DecoratedBox(
        decoration: const BoxDecoration(
          gradient: RadialGradient(
            center: Alignment.topCenter,
            radius: 1.25,
            colors: [Color(0xff102844), BotColors.background],
          ),
        ),
        child: child,
      );
}
