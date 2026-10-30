# Projection Mapping Studio

Windows C++20 + Qt 6 멀티 프로젝터 맵핑 프로그램.

## v0.8.0 — 영상별 재생 시간 제한

- Media 탭에서 영상 항목을 선택한 뒤 **Selected video playback time**에 재생 시간을 초 단위로 입력합니다.
- **0초(Full video)**는 영상이 끝날 때까지 재생합니다. **20초**는 재생 위치가 20초에 도달하면 다음 항목으로 넘어갑니다.
- 영상마다 서로 다른 제한 시간을 JSON v5로 저장합니다. 기존 v4 재생목록은 시간 제한 0초로 호환됩니다.
- 시작 구간 선택, 속도, 페이드 전환은 이번 변경에 포함되지 않습니다.
- 이전 v0.7.0의 MSVC std::min 정수형 불일치 컴파일 오류도 수정했습니다.

## v0.7.0 — 재생목록 및 영상별 회전/비율 설정

- Media 탭에서 **Add videos...**로 여러 영상을 한꺼번에 등록
- 목록의 각 영상을 선택해 0/90/180/270도 회전 및 Stretch/Fit/Fill을 개별 지정
- Play selected 후 영상이 끝나면 자동 다음 영상으로 진행, Repeat entire playlist 옵션으로 전체 반복
- Previous/Next/Pause/Stop, Remove, Move up/Move down 제공
- Save settings에서 재생목록 경로, 순서, 개별 회전/비율, 반복 옵션을 JSON v4로 저장. 이전 v1~v3 설정도 열 수 있음
- 영상 파일 자체는 JSON에 포함되지 않으므로 원본 경로를 유지해야 함
- 출력 채널 2개는 동일한 QMediaPlayer 프레임을 공유하고 기존 워핑/블렌딩 보정은 계속 사용
- **성능 주의:** 90° 회전은 아직 CPU 변환이므로 4K 영상에서 렉이 발생할 수 있음. 사전 로딩/무중단 전환도 추후 과제

## v0.6.1 — 세로형 영상 회전 및 비율 설정

**Media** 탭에 아래 기능을 추가했습니다.

- 영상 회전: 0° / 90° / 180° / 270°
- 비율 처리:
  - **Stretch**: 이전 버전과 동일하게 가상 가로 화면을 가득 채움 (영상 비율 왜곡 가능)
  - **Fit**: 원본 비율을 유지하고 영상 전체를 보여줌 (남는 부분 검은 여백)
  - **Fill**: 원본 비율을 유지하면서 화면을 가득 채움 (넘치는 부분 잘림)
- 영상 파일은 수정하지 않으며, 재생할 때 메모리에서 변환
- 기존 Qt CPU 및 실험적 Direct3D 11 GPU 출력 양쪽에 동일한 변환된 영상 사용
- 변경된 회전·비율 설정은 JSON에 저장 (v3 형식, 구형 v1/v2 불러오기 가능)
- Windows CI에 영상 회전/맞춤 단위 테스트 추가

**주의:** 현재 회전·비율 변환은 두 렌더러에 공통으로 사용하기 위해 Qt 이미지 처리 경로에서 수행합니다. GPU 렌더러라도 이 단계는 CPU 처리를 사용하므로, 대형 4K 영상에서 높은 성능은 보장되지 않습니다. 최종 GPU 셰이더 기반 회전 및 zero-copy 영상 처리는 추후 최적화 과제입니다.

가상 합성 화면은 2560×720, 두 프로젝터를 나란히 놓은 32:9 비율을 기준으로 합니다. 실제 바닥의 물리적 화면 비율에 맞춘 사용자 정의 캔버스 크기는 추후 추가할 예정입니다.

## 빌드

- Visual Studio 2022 MSVC 64-bit, Windows SDK
- Qt 6.12.0 MSVC 2022 64-bit + Multimedia
- CMake 3.24+

```powershell
git pull origin main
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/msvc2022_64"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
& "C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe" --release ".\build\Release\ProjectionMappingStudio.exe"
.\build\Release\ProjectionMappingStudio.exe
```

## 사용 방법

1. Projectors 탭에서 출력 장치 선택 후 테스트 격자를 열어 보정 상태 확인
2. Media 탭에서 세로 MP4 파일 선택
3. **Video rotation**을 90° 또는 270°로 설정
4. **Aspect ratio**에서 Fit(전체 보기) 또는 Fill(화면 채우기) 선택
5. Play on both projector outputs로 재생
6. 설정을 유지하려면 Save settings로 JSON 저장

GPU renderer 탭의 실험적 Direct3D 모드는 기본적으로 꺼져 있습니다. 기본 Qt 출력은 그대로 사용 가능합니다.
