# Projection Mapping Studio

Windows용 C++20 / Qt 6 기반 멀티 프로젝터 맵핑 프로토타입.

## v0.3 새 기능

- 프로젝터별 **4점 퍼스펙티브 워핑**: Mapping calibration 탭에서 각 꼭짓점을 움직이면 실제 전체 화면 출력에도 즉시 반영
- 두 프로젝터가 공유하는 원본 영상 구간의 **밝기 페이드**: 겹침 폭(0~40%)에 따른 좌우 가중치 조정
- 출력 밝기 슬라이더 실시간 반영
- 프로젝터별 격자 테스트를 실제 변환으로 표시
- 기존 v0.2의 디스플레이 선택, MP4 파일 재생, 반복 재생, JSON 설정 저장/로드 유지
- Windows Actions 빌드 및 실행 파일 번들

### 제한 사항

이 버전은 **Qt QPainter 기반 CPU 프로토타입**입니다. 프로젝트ive 워핑과 화면별 밝기 페이드가 작동하지만, 하드웨어 동기화·GPU Direct3D 11/HLSL 최적화·정밀 감마/블랙레벨/색상 보정은 아직 없습니다. 두 프로젝터의 **실제 빛이 겹치는 영역**을 현장에서 먼저 정렬해야 합니다. 전형적인 밝기 페이드만으로 완전히 보이지 않는 경계가 보장되지는 않습니다.

프로젝터 보정점은 각 출력 화면의 **정규화 좌표(0~1)** 이며, 두 프로젝터를 각각 선택해 독립적으로 조정합니다. v0.2 설정 JSON을 가져왔다면 이전 좌표계에 맞춘 값이므로 **Reset calibration** 후 다시 설정하세요.

## 환경

- Windows 10/11, Visual Studio 2022, CMake 3.24+
- Qt 6.5 이상 MSVC 2022 64-bit, **Qt Multimedia** 모듈
- Windows 디스플레이 설정: **확장** 모드
- 권장: 관리용 모니터 1대 + 빔프로젝터 2대

## 빌드 (Qt 6.12.0)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/msvc2022_64"
cmake --build build --config Release
& "C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe" --release ".\build\Release\ProjectionMappingStudio.exe"
.\build\Release\ProjectionMappingStudio.exe
```

## 현장 보정 절차

1. **Projectors** 탭에서 서로 다른 두 프로젝터 디스플레이 선택
2. **Open warped test patterns** 실행
3. **Mapping calibration**에서 Projector 1 선택 후 네 꼭짓점을 움직여 첫 번째 투사 영역 정렬
4. Projector 2 선택 후 똑같이 정렬
5. 두 프로젝터의 영상이 겹치도록 실제 설치 위치를 조정하고 **Overlap width**로 공유 영상 폭 조정
6. 실제 바닥에서 겹침 영역의 밝기를 확인하며 두 위치와 밝기를 미세 조정
7. **Save settings**로 저장하고 **Media** 탭에서 MP4 선택 후 재생

*Esc*로 프로젝터 출력창을 닫거나 Projectors 탭의 Close projector windows 사용.

## 다음 개발

- Direct3D 11 / HLSL GPU 워프·블렌드 파이프라인
- 감마·블랙레벨·색상 차이 정밀 보정
- 프로젝터 장치 식별자 지속 저장, 분리/재연결 대응
- 디스플레이 프레젠테이션 시각 동기화와 장시간 안정성 테스트
