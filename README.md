# Projection Mapping Studio

Windows용 C++20 / Qt 6 멀티 프로젝터 매핑 프로토타입.

## v0.4 기능

- Windows 확장 디스플레이에서 프로젝터 두 대 선택 및 출력
- Qt Multimedia 단일 영상 타임라인 → 좌우 영상 영역 동시 출력
- 4점 실시간 퍼스펙티브 워핑 및 보정 격자
- 겹침 영역 밝기 페이드 및 블렌딩 감마(1.0~3.5) 조절
- 프로젝터별 RGB 감쇠(0~100%)로 색상/밝기 맞춤
- JSON 보정값 저장·로드 (v1 및 v2 설정 파일 읽기 지원)
- GitHub Actions Windows 빌드 및 실행 파일 ZIP 아티팩트

### 중요 제약

현재 구현은 Qt QPainter CPU 영상 처리 방식입니다. Direct3D 11/HLSL 최적화, 하드웨어 프레임 동기화 및 정밀 컬러 캘리브레이션은 별도 개발이 필요합니다. RGB는 밝기를 줄일 수만 있고 100% 이상 증폭하지 않습니다. **블랙 레벨 보정은 미구현**이며 프로젝터의 광학적 검은색 누설광을 소프트웨어가 제거할 수 없습니다.

### 빌드

Visual Studio 2022, Qt 6.12.0 **MSVC 2022 64-bit + Multimedia**, CMake 3.24+가 필요합니다.

```powershell
git pull origin main
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/msvc2022_64"
cmake --build build --config Release
& "C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe" --release ".\build\Release\ProjectionMappingStudio.exe"
.\build\Release\ProjectionMappingStudio.exe
```

### 현장 사용법

1. Windows 디스플레이 확장 모드. 노트북 관리자 화면 외 프로젝터 2대 선택.
2. Projectors → **Open warped test patterns**.
3. Mapping calibration → Projector 1/2 각각 네 꼭짓점을 움직여 실제 격자 정렬.
4. **Save settings**로 현재 좌표 백업.
5. Media → 영상 파일 선택·재생. 밝은 장면에서 Overlap width를 조절.
6. **Color & Blend** → Blend gamma(처음 2.2) 조정. 경계가 밝으면 인접 구간을 어둡게, 어두우면 반대로 조절.
7. Color & Blend → Projector 1/2의 Red/Green/Blue 값을 조금씩 낮춰 색상·밝기 일치.
8. 마지막 설정 저장. 이전 v0.3 JSON은 읽기 지원하며 새 보정값은 v2 형식으로 저장.

### 다음 과제

- Direct3D 11 GPU 파이프라인
- 정밀 감마, 블랙 레벨, 색상 교정 및 프로젝터별 LUT
- EDID 기반 디스플레이 연결 안정화 및 재연결
- 장시간 재생·현장 캘리브레이션 검증
