# WITH OUT CMC Integration

CMC 슈터 템플릿의 시험 이식과 기존 플레이어 이동 전환을 위한 프로젝트 전용 플러그인이다.

## 기존 플레이어 연결 (2차)

- 실제 `/Game/Blueprint/BP_ProtoCharacter`의 부모를 `SandboxCharacter_CMC`로 변경했다.
- 상속 구조는 `BP_ProtoCharacter -> SandboxCharacter_CMC -> AProtoCharacter`다.
  기존 캐릭터 캐스팅, 인벤토리/장비/퀵슬롯/생존 수치 C++은 유지한다.
- 기본 맵과 기존 GameMode는 바꾸지 않았다. 원래 플레이어가 생성되는 맵에도 적용된다.
- 플레이어 메시는 UEFN 마네킹, Anim Class는 `SandboxCharacter_CMC_ABP`로 전환했다.
- 원래 BP의 BeginPlay 실행 흐름 앞에 CMC 부모 BeginPlay 호출을 연결했다.
  기존 컨트롤러를 유지하므로 C++에서 CMC 카메라 초기화도 호출한다.
- 프로젝트 모듈은 PostDefault에 로드한다. 기존 생성자의 애셋 참조가 CMC 부모를
  통해 DrawDebugLibrary/ProxyTable 등을 플러그인 초기화 전에 읽지 않도록 하기 위함이다.
- CMC가 이동/시점/점프/웅크리기를, 기존 C++이 무기/인벤토리/상호작용을 처리한다.
  매핑은 실행 시 복제해서 필터링하며 기존 입력 애셋 자체는 변경하지 않는다.
- 기존 스태미나 제한이 스프린트에 적용되며 조준 상태도 CMC 입력 상태에 전달한다.
- 기존 WeaponSocket/PistolSocket/WeaponStorage/PistolStorage를 새 메시의 동일 뼈에 복사했다.
- 기존 인벤토리를 사용하며 CMC InventoryManager나 IMC_Shooter를 실제 플레이어에 추가하지 않는다.
- 서버와 프로토콜, 동료 AI 코드는 변경하지 않았다.

### 확인

`/Game/CMCIntegration/Maps/L_ProtoCMCPlayerAcceptance`는 장애물 없는 플레이어 확인용 맵이다.
WASD 이동, Shift 스프린트, Space 점프, C 웅크리기, Ctrl 걷기, 마우스 시점 회전.
기존 키는 I 인벤토리, F 상호작용, 1/2 무기 선택, 3 수납, R 장전, 우클릭 조준/좌클릭 발사다.
이전 스프링암용 휠 줌은 CMC 카메라에서 적용하지 않는다.

자동 테스트: `WITHOUT.ProtoCMC.Player.GameplayAndMovement`.
네이티브 캐릭터 상속, 기존 인벤토리/장비 컴포넌트 유지, CMC Anim Class,
실제 키 입력으로 이동/스프린트/조준/기존 인벤토리 열기/점프/웅크리기를 검사한다.
테스트는 헤드리스이므로 최종 애니메이션 외형과 카메라 구도는 에디터에서 확인해야 한다.
2026-11-26 최종 확인: Development Editor 빌드와 실제 BP_ProtoCharacter PIE 테스트 통과.
걷기 최대 속도 500, 스프린트 700. 새 실행의 Blueprint/스크립트 모듈 오류가 없음을 확인했다.

### 남은 작업

이 단계는 기존 플레이어의 이동과 기본 애니메이션 전환이다. 기존 총 로직은 유지하지만,
기존 무기용 상체 IK/발사/장전/스왑 몽타주를 CMC 애니메이션 레이어와 새 스켈레톤/슬롯에
맞추는 작업까지 완료한 것은 아니다. 템플릿 총/인벤토리로 대체하지 않는다.
멀티플레이 이동/시점 및 서버 연결 회귀 검증도 별도 필요하다.

이 아래는 별도 샘플 캐릭터를 사용했던 1차 이식 기록이다.

## 테스트 맵

프로토타입에서 Content/CMCIntegration/Maps/L_CMCIntegrationTest를 열고 Play한다.
기본 타이틀 맵이나 기존 게임 맵에서는 원래 캐릭터와 GameMode를 계속 사용한다.

- BP_CMCIntegrationCharacter: 템플릿 ShooterCharacter_CMC의 자식.
- BP_CMCIntegrationGameMode: 템플릿 GM_Sandbox의 자식이며 CMC 캐릭터와 PC_Shooter를 지정.
- 원본 템플릿 애셋은 /CMCIntegration 마운트에 있다. 콘텐츠 브라우저에서
  플러그인 콘텐츠 표시를 켜면 확인할 수 있다.
- 이동, 조준, 무기 습득, 발사, 장전, 무기 교체는 템플릿 방식으로 시험한다.
  정확한 키와 현재 입력 모드는 템플릿의 컨트롤 안내를 확인한다.

## 1차에서 하지 않았던 작업

- 기존 AProtoCharacter 상속 계층과의 호환 연결
- 기존 인벤토리, 생존 수치, 퀵슬롯, 적 피해 처리 연결
- 커스텀 서버 프로토콜 연결이나 동료 AI 교체
- 새 몽타주 제작, 기존 플레이어 애셋 덮어쓰기, 기본 맵 교체

템플릿 사격과 기존 총알 처리를 동시에 켜지 않는다. 현재는 기존 적 피해와
별도로 움직이는 테스트 환경이다.

## 설정

플러그인 의존성은 CMCIntegration.uplugin에 선언했다. 입력 애셋 이름이 겹쳐도
기존 /Game/Input 파일을 덮어쓰지 않는다. Gameplay Tags 테이블 경로는 플러그인으로
변환했고, 새 Weapon/Traversable/Interaction/Obstacle 충돌 채널을 병합했다.
CommonUI용 GameViewportClient 설정은 프로젝트 전체에 적용되므로 기존 UI의 입력도
후속 회귀 테스트 대상이다. 기존 맵과 GameMode 기본값은 변경하지 않았다.

## 검증

에디터 자동 테스트:
- WITHOUT.CMCIntegration.PIE.Movement
- WITHOUT.CMCIntegration.PIE.Shooting

이동 테스트는 전용 맵을 열고 PIE에서 CMC 캐릭터를 소유한 뒤 W 입력으로 이동하는지
확인한다. 사격 테스트는 템플릿의 AddElementByActor/TryEquipElement API로 일회성
라이플을 장착하고, 테스트 탄약 10발을 설정한 다음 실제 조준/발사 마우스 입력으로
탄약이 줄어드는지 확인한다. 테스트 탄약은 PIE 종료 후 남지 않는다.

2026-11-26: UE 5.8 Development Editor 빌드 및 두 PIE 테스트 통과.
이동 602.9cm, 라이플 탄약 10발에서 5발로 감소. 주요 블루프린트 7개 컴파일 통과.
헤드리스 검증이므로 애니메이션 외형, 장전 전체 흐름, 오디오, 다른 입력 장치,
기존 게임 UI와 멀티플레이의 실제 동작은 별도 확인이 필요하다.

원본 템플릿에도 없는 참조 15개가 검사에서 발견됐다. Echo, Twinblast, MetaHuman,
이전 위젯 및 일부 애니메이션 경로가 포함된다. 실제 원본 파일이 존재하는 의존성의
이식 누락과는 구별해서 기록했다. 선택 캐릭터와 권총 장전 등의 실제 동작은 별도
확인이 필요하며, 이 참조들을 정상 애셋으로 간주하거나 자동 삭제하지 않았다.

## Git

이식 애셋은 약 4GB다. 이 플러그인의 Content는 Git LFS로 추적하도록 지정했다.
커밋과 푸시는 자동으로 하지 않는다. 팀 저장소에 올리기 전 LFS 저장 공간과
전송 한도를 확인한다.
